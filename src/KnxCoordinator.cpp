// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name KnxCoordinator.cpp
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details See KnxCoordinator.h.
*/

//----Libraries----
#include "KnxCoordinator.h"

//---- Sending ----

bool KnxCoordinator::send(uint16_t groupAddress, const KnxValue& value) {
	if (KnxDebug::isDeep()) logTransmitStart(groupAddress, value);

	uint8_t frame[KnxFrame::MAX_FRAME];
	uint8_t len = KnxFrame::build(physicalAddress, groupAddress, value, frame, sizeof(frame));
	if (len == 0) return false;   // build() said why
	return transmit(frame, len, &value);
}

bool KnxCoordinator::sendIndividual(PhysicalAddress target, uint16_t apci,
                                    const uint8_t* payload, uint8_t payloadLength) {
	KnxDebug::section("TX %u.%u.%u -> %u.%u.%u  APCI 0x%03X",
		(unsigned)physicalAddress.area, (unsigned)physicalAddress.line,
		(unsigned)physicalAddress.device, (unsigned)target.area, (unsigned)target.line,
		(unsigned)target.device, (unsigned)apci);

	uint8_t frame[KnxFrame::MAX_FRAME];
	uint8_t len = KnxFrame::buildIndividual(physicalAddress, target, apci,
		payload, payloadLength, frame, sizeof(frame));
	if (len == 0) return false;
	return transmit(frame, len, nullptr);
}

bool KnxCoordinator::sendControl(PhysicalAddress target, KnxTpci tpci, uint8_t sequenceNumber) {
	KnxDebug::section("TX %u.%u.%u -> %u.%u.%u  control %u",
		(unsigned)physicalAddress.area, (unsigned)physicalAddress.line,
		(unsigned)physicalAddress.device, (unsigned)target.area, (unsigned)target.line,
		(unsigned)target.device, (unsigned)tpci);

	uint8_t frame[KnxFrame::MAX_FRAME];
	uint8_t len = KnxFrame::buildControl(physicalAddress, target, tpci, sequenceNumber,
		frame, sizeof(frame));
	if (len == 0) return false;
	return transmit(frame, len, nullptr);
}

bool KnxCoordinator::transmit(const uint8_t* frame, uint8_t len, const KnxValue* value) {
	bool confirmed = p_driver->sendTelegram(frame, len);
	if (KnxDebug::isEnabled()) logTransmitted(frame, len, value, confirmed);
	return confirmed;
}

// Formatting lives in its own non-inlined functions so their buffers only take stack while
// logging — send() is often called from inside a receive callback.
KNX_NOINLINE void KnxCoordinator::logTransmitStart(uint16_t groupAddress, const KnxValue& value) {
	GroupAddress ga = unpackGroupAddress(groupAddress);
	char text[40];
	KnxFormat::value(value, text, sizeof(text));
	KnxDebug::section("TX %u.%u.%u -> %u/%u/%u  Write  (%s)",
		(unsigned)physicalAddress.area, (unsigned)physicalAddress.line,
		(unsigned)physicalAddress.device, (unsigned)ga.main, (unsigned)ga.middle,
		(unsigned)ga.sub, text);
}

KNX_NOINLINE void KnxCoordinator::logTransmitted(const uint8_t* frame, uint8_t len,
                                                 const KnxValue* value, bool confirmed) {
	if (KnxDebug::isDeep()) {
		if (confirmed) KnxDebug::log("result: confirmed by the bus");
		else           KnxDebug::problem("not confirmed by the bus");
		// Sent from a receive callback: the rest of the received telegram's block follows.
		if (_inLoop) KnxDebug::section("RX continued");
		return;
	}

	char line[64];
	line[0] = '\0';
	ParsedTelegram sent;
	if (KnxFrame::parse(frame, len, sent)) KnxFormat::telegram(sent, line, sizeof(line));
	char text[40];
	text[0] = '\0';
	if (value != nullptr) KnxFormat::value(*value, text, sizeof(text));
	if (confirmed) {
		KnxDebug::info("TX  %s%s%s%s  confirmed", line, text[0] ? "  (" : "", text, text[0] ? ")" : "");
	}
	else {
		KnxDebug::problem("TX  %s  not confirmed by the bus", line);
	}
}

//---- Receiver registry (intrusive singly-linked list) ----

void KnxCoordinator::registerReceiver(IKnxReceiver* receiver) {
	if (!receiver) return;
	// Idempotent: don't double-link an already-registered receiver.
	for (IKnxReceiver* n = receiverHead; n != nullptr; n = n->nextReceiver) {
		if (n == receiver) return;
	}
	receiver->nextReceiver = receiverHead;
	receiverHead = receiver;
}

void KnxCoordinator::unregisterReceiver(IKnxReceiver* receiver) {
	IKnxReceiver** link = &receiverHead;
	while (*link != nullptr) {
		if (*link == receiver) {
			*link = receiver->nextReceiver;
			receiver->nextReceiver = nullptr;
			return;
		}
		link = &(*link)->nextReceiver;
	}
}

void KnxCoordinator::dispatch(const ParsedTelegram& telegram) {
	// A group telegram whose APDU stops after the TPCI octet carries no application service,
	// so there is nothing for a receiver to decode. Structurally legal, semantically empty.
	if (!telegram.hasApci) {
		KnxDebug::log("no application data, ignored");
		return;
	}

	uint16_t ga = packGroupAddress(telegram.target);

	// Catch-all hook first: it sees every group telegram, reads and foreign traffic included.
	if (_hookHasContext ? telegramHook.withContext != nullptr : telegramHook.plain != nullptr) {
		KnxTelegram event(telegram);
		if (_hookHasContext) telegramHook.withContext(event, p_hookContext);
		else                 telegramHook.plain(event);
	}

	uint16_t delivered = dispatchSubscriptions(telegram, ga);
	for (IKnxReceiver* r = receiverHead; r != nullptr; r = r->nextReceiver) {
		if (r->matches(ga)) {
			r->receive(telegram);
			delivered++;
		}
	}

	// "0 listeners" is normal foreign traffic, not an error.
	KnxDebug::log("-> delivered to %u listener(s)", (unsigned)delivered);
}

//---- Subscriptions ----

bool KnxCoordinator::setSubscriptionSlotLimit(uint16_t total) {
	if (p_subscriptions != nullptr) return false;   // table already reserved
	// On AVR size_t is 16-bit: a huge total would wrap the calloc size to a tiny block.
	if ((size_t)total > ((size_t)-1) / sizeof(Subscription)) return false;
	subscriptionLimit = total;
	return true;
}

bool KnxCoordinator::addSubscription(uint16_t groupAddress, KnxDpt dpt, TelegramHandler handler,
                                     uint8_t flags, void* context) {
	if (handler.plain == nullptr || subscriptionLimit == 0) return false;

	// Reserved exactly once and never freed or resized, so the heap cannot fragment.
	if (p_subscriptions == nullptr) {
		p_subscriptions = (Subscription*)calloc(subscriptionLimit, sizeof(Subscription));
		if (p_subscriptions == nullptr) {
			KnxDebug::problem("no memory for %u subscription slots", (unsigned)subscriptionLimit);
			return false;
		}
	}

	for (uint16_t i = 0; i < subscriptionLimit; i++) {
		Subscription& slot = p_subscriptions[i];
		if (slot.flags & SLOT_USED) continue;
		slot.groupAddress = groupAddress;
		slot.dpt          = dpt;
		slot.handler      = handler;
		slot.context      = context;
		slot.flags        = (uint8_t)(SLOT_USED | flags);
		return true;
	}
	KnxDebug::problem("all %u subscription slots in use (see setSubscriptionSlotLimit)", (unsigned)subscriptionLimit);
	return false;
}

void KnxCoordinator::unsubscribe(uint16_t groupAddress) {
	if (p_subscriptions == nullptr) return;
	// Slots are only marked free, never moved, so a dispatch in progress stays consistent.
	for (uint16_t i = 0; i < subscriptionLimit; i++) {
		Subscription& slot = p_subscriptions[i];
		if ((slot.flags & SLOT_USED) && slot.groupAddress == groupAddress) slot.flags = 0;
	}
}

uint16_t KnxCoordinator::dispatchSubscriptions(const ParsedTelegram& telegram, uint16_t groupAddress) {
	if (p_subscriptions == nullptr) return 0;
	// A read request carries no value; subscribers only hear values.
	if (telegram.type != GroupValueType::Write && telegram.type != GroupValueType::Response) return 0;

	uint16_t fired = 0;
	for (uint16_t i = 0; i < subscriptionLimit; i++) {
		Subscription& slot = p_subscriptions[i];
		if (!(slot.flags & SLOT_USED) || slot.groupAddress != groupAddress) continue;

		KnxValue value = KnxCodec::decodeTelegram(slot.dpt, telegram);
		if (!value.isValid()) {
			GroupAddress g = unpackGroupAddress(groupAddress);
			KnxDebug::problem("subscription on %u/%u/%u: value does not decode as DPT %u",
				(unsigned)g.main, (unsigned)g.middle, (unsigned)g.sub, (unsigned)slot.dpt);
			continue;
		}

		if (KnxDebug::isDeep()) logSubscriptionHit(value);
		KnxTelegram event(telegram, value);
		if (slot.flags & SLOT_HAS_CONTEXT) slot.handler.withContext(event, slot.context);
		else                               slot.handler.plain(event);
		fired++;
	}
	return fired;
}

void KnxCoordinator::dispatchIndividual(const ParsedTelegram& telegram) {
	// Point-to-point telegrams have exactly one legitimate recipient. Anything not addressed
	// to us is ordinary foreign traffic on a shared bus, not an error.
	bool forUs = paEqual(telegram.individualTarget, physicalAddress);

	KnxDebug::log("-> %s", !forUs ? "for another device" : (p_deviceHandler ? "device handler" : "for us, no handler"));

	if (forUs && p_deviceHandler) p_deviceHandler->receiveIndividual(telegram);
}

//---- Logging ----

uint16_t KnxCoordinator::countListeners(const ParsedTelegram& telegram, KnxDpt& displayDpt) const {
	uint16_t ga = packGroupAddress(telegram.target);
	uint16_t count = 0;
	displayDpt = KnxDpt::UNKNOWN;
	bool carriesValue = telegram.type == GroupValueType::Write || telegram.type == GroupValueType::Response;

	if (p_subscriptions != nullptr && carriesValue) {
		for (uint16_t i = 0; i < subscriptionLimit; i++) {
			const Subscription& slot = p_subscriptions[i];
			if (!(slot.flags & SLOT_USED) || slot.groupAddress != ga) continue;
			if (displayDpt == KnxDpt::UNKNOWN) displayDpt = slot.dpt;
			count++;
		}
	}
	for (IKnxReceiver* r = receiverHead; r != nullptr; r = r->nextReceiver) {
		if (!r->matches(ga)) continue;
		if (displayDpt == KnxDpt::UNKNOWN) displayDpt = r->receiverDpt();
		count++;
	}
	if (!carriesValue) displayDpt = KnxDpt::UNKNOWN;   // a read has no value to show
	return count;
}

KNX_NOINLINE void KnxCoordinator::logReceived(const ParsedTelegram& telegram, uint8_t len) {
	char line[64];
	KnxFormat::telegram(telegram, line, sizeof(line));

	if (telegram.addressType == KnxAddressType::Individual) {
		bool forUs = paEqual(telegram.individualTarget, physicalAddress);
		if (KnxDebug::isDeep()) KnxDebug::section("RX %s", line);
		else KnxDebug::info("RX  %s  (%s)", line, forUs ? "for this device" : "for another device");
	}
	else {
		KnxDpt dpt;
		uint16_t listeners = countListeners(telegram, dpt);
		char text[40];
		text[0] = '\0';
		if (dpt != KnxDpt::UNKNOWN) {
			KnxValue shown = KnxCodec::decodeTelegram(dpt, telegram);   // silent on failure
			KnxFormat::value(shown, text, sizeof(text));
		}
		const char* open  = text[0] ? "  (" : "";
		const char* close = text[0] ? ")" : "";
		if (KnxDebug::isDeep()) {
			KnxDebug::section("RX %s%s%s%s", line, open, text, close);
		}
		else if (listeners == 0) {
			KnxDebug::info("RX  %s%s%s%s  no listener", line, open, text, close);
		}
		else {
			KnxDebug::info("RX  %s%s%s%s  %u listener%s", line, open, text, close,
				(unsigned)listeners, listeners == 1 ? "" : "s");
		}
	}

	if (!KnxDebug::isDeep()) return;
	KnxDebug::logBytes("RX  frame:", rxFrame, len);
	char detail[48];
	KnxFormat::frameControl(rxFrame, len, detail, sizeof(detail));
	KnxDebug::log("%s", detail);
	KnxFormat::frameRouting(rxFrame, len, detail, sizeof(detail));
	KnxDebug::log("%s", detail);
}

KNX_NOINLINE void KnxCoordinator::logSubscriptionHit(const KnxValue& value) {
	char text[40];
	KnxFormat::value(value, text, sizeof(text));
	KnxDebug::log("subscription -> %s", text);
}

//---- Receiving ----

bool KnxCoordinator::loop(void) {
	// A callback that calls loop() again would overwrite rxFrame while the telegram it was
	// handed still points into it; the nested call is a no-op instead.
	if (_inLoop) return false;
	_inLoop = true;

	bool processed = false;
	uint8_t len = 0;

	// Drain every complete frame the driver has reassembled this cycle.
	while (p_driver->poll(rxFrame, sizeof(rxFrame), len)) {
		ParsedTelegram telegram;
		if (!KnxFrame::parse(rxFrame, len, telegram)) {
			KnxDebug::section("RX unreadable frame");
			KnxDebug::logBytes("RX  frame:", rxFrame, len);
			KnxDebug::problem("RX  frame dropped: %s (%u bytes)", KnxFrame::lastParseError(), (unsigned)len);
			continue;
		}

		// The log line comes first, so a callback's own output appears under its telegram.
		if (KnxDebug::isEnabled()) logReceived(telegram, len);

		if (telegram.addressType == KnxAddressType::Individual) {
			dispatchIndividual(telegram);
		}
		else {
			dispatch(telegram);
		}
		processed = true;
	}
	_inLoop = false;
	return processed;
}
