// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name KnxDriver.cpp
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details See KnxDriver.h. Adapted from the thesis KNX_TPUART2 driver; the telegram-layer
 *          coupling is removed (RX is exposed as reassembled frames via poll()) and the send
 *          path now reads the real L_Data.con instead of hard-returning true (PLAN §9).
 *          Bring-up is chip-specific and lives in KnxBringUp; this file only pumps it.
*/

//----Libraries----
#include "KnxDriver.h"

#ifdef KNX_DEFAULT_PORT
KnxDriver::KnxDriver(String physicalAddress, KnxTransceiver chip)
	: KnxDriver(physicalAddress, KNX_DEFAULT_PORT, chip) {}
#endif

// The port is remembered twice on purpose: as a Stream for the byte traffic, and as a
// HardwareSerial so begin() knows it may configure the line.
KnxDriver::KnxDriver(String physicalAddress, HardwareSerial& port, KnxTransceiver chip)
	: p_io(&port), p_uart(&port),
	  physicalAddress(physicalAddressFromString(physicalAddress)), chip(chip) {}

// Stream path: the caller owns the line settings, so p_uart stays null.
KnxDriver::KnxDriver(String physicalAddress, Stream& stream, KnxTransceiver chip)
	: p_io(&stream), p_uart(nullptr),
	  physicalAddress(physicalAddressFromString(physicalAddress)), chip(chip) {}

//---- Status-pin interrupts ----
// The ISRs only latch an edge; the reaction (callback, bring-up) runs from loop(). User code in
// an ISR is unsafe — on ESP32 a flash write or Serial print from interrupt context crashes —
// and a bring-up needs UART traffic and timeouts. Latching still guarantees that no edge is
// missed, however short the pulse or slow the loop. A free ISR cannot carry an object pointer,
// so the latches are file-wide: interrupts serve one driver with status pins per sketch.
#ifndef ARDUINO_ISR_ATTR
	#define ARDUINO_ISR_ATTR   // ESP32 core: places the ISR in IRAM; elsewhere empty
#endif

static volatile bool saveFellLatch  = false;
static volatile bool resetRoseLatch = false;

static void ARDUINO_ISR_ATTR onSaveFalling(void)  { saveFellLatch  = true; }
static void ARDUINO_ISR_ATTR onResetRising(void)  { resetRoseLatch = true; }

// Attaches isr to pin (FALLING or RISING edge) if the pin can interrupt; returns false where
// it cannot, and the pin is polled instead. The mode constant is written out at each call
// rather than passed in: ArduinoCore-API cores (Renesas, Mbed, RP2040) type it as PinStatus,
// the others as int, and only the bare constant converts to both.
static bool attachIfPossible(int pin, void (*isr)(void), bool rising) {
	const int irq = (int)digitalPinToInterrupt(pin);
	if (irq < 0) return false;
	if (rising) attachInterrupt(irq, isr, RISING);
	else        attachInterrupt(irq, isr, FALLING);
	return true;
}

// Reads and clears a latch with interrupts off, so an edge between read and clear is not lost.
static bool takeLatch(volatile bool& latch) {
	noInterrupts();
	const bool set = latch;
	latch = false;
	interrupts();
	return set;
}

//---- Private methods ----

void KnxDriver::sendCommand(const uint8_t* cmd, uint16_t len) {
	p_io->write(cmd, len);
}

void KnxDriver::setStatusPins(int resetPin, int savePin) {
	this->resetPin = resetPin;
	this->savePin  = savePin;
}

bool KnxDriver::busPowerOk(void) const {
	return savePin < 0 || digitalRead(savePin) == HIGH;
}

bool KnxDriver::chipInReset(void) const {
	return resetPin >= 0 && digitalRead(resetPin) == LOW;
}

bool KnxDriver::prepareChip(void) {
	if (resetPin < 0) return true;

	if (chip == KnxTransceiver::TpUart2) {
		// RESn is an input as well as an output on the TP-UART2: forcing it low resets the
		// chip. Emulate open drain — drive low, then release; never drive it high.
		digitalWrite(resetPin, LOW);
		pinMode(resetPin, OUTPUT);
		digitalWrite(resetPin, LOW);
		delay(RESET_PULSE_MS);
		pinMode(resetPin, INPUT);
		KnxDebug::log("DRV tpuart2 hard reset via RESn");
	}
	// On the NCN5130 RESETB is an output only: it is low while the bus voltage is too low,
	// the chip is too hot or its watchdog fired, and no UART traffic is possible then.

	const uint32_t start = millis();
	while (digitalRead(resetPin) == LOW) {
		if (millis() - start >= READY_TIMEOUT_MS) {
			KnxDebug::log("DRV !! reset pin still low after %u ms — bus voltage present?",
				(unsigned)READY_TIMEOUT_MS);
			return false;
		}
	}
	if (chip == KnxTransceiver::TpUart2) delay(TPUART2_SETTLE_MS);
	lastResetHigh = true;
	return true;
}

bool KnxDriver::checkStatusPins(void) {
	if (savePin >= 0) {
		const bool high = digitalRead(savePin) == HIGH;
		// Edge from the interrupt latch, or seen by polling where the pin cannot interrupt.
		if (takeLatch(saveFellLatch) || (!high && lastSaveHigh)) {
			KnxDebug::log("DRV !! save pin low: bus power failing");
			if (p_onPowerFail != nullptr) p_onPowerFail();
		} else if (high && !lastSaveHigh) {
			KnxDebug::log("DRV save pin high again: bus power ok");
		}
		lastSaveHigh = high;
	}

	if (resetPin < 0) return true;
	const bool high = digitalRead(resetPin) == HIGH;
	const bool rose = takeLatch(resetRoseLatch);
	if (!high) {
		if (lastResetHigh) KnxDebug::log("DRV !! chip in reset (reset pin low)");
		lastResetHigh = false;
		return false;
	}
	// A reset pulse shorter than one loop() pass is only visible through the latch.
	if (!lastResetHigh || rose) {
		// The chip came back from a reset and has forgotten its address: bring it up again.
		lastResetHigh = true;
		KnxDebug::log("DRV reset pin high again: re-running bring-up");
		if (chip == KnxTransceiver::TpUart2) delay(TPUART2_SETTLE_MS);
		bringUp("recover");
	}
	return true;
}

// Readable name of a bring-up step, for the debug log.
static const char* stepName(KnxBringUp::Step s) {
	switch (s) {
		case KnxBringUp::Step::Reset:          return "reset";
		case KnxBringUp::Step::SystemStatus:   return "system status";
		case KnxBringUp::Step::SetAddress:     return "set address";
		case KnxBringUp::Step::AwaitConfigure: return "address confirmation";
		case KnxBringUp::Step::State:          return "state";
		default:                               return "-";
	}
}

bool KnxDriver::bringUp(const char* phase) {
	const bool ncn = (chip == KnxTransceiver::Ncn5130);
	const char* chipName = ncn ? "ncn5130" : "tpuart2";

	bringUpSm.start(chip, physicalAddress, millis());
	while (!bringUpSm.finished()) {
		const uint8_t* cmd;
		uint8_t len;
		if (bringUpSm.nextCommand(cmd, len)) sendCommand(cmd, len);
		while (p_io->available()) bringUpSm.feed((uint8_t)p_io->read(), millis());
		bringUpSm.tick(millis());
	}
	reassembler.reset();   // frames seen during bring-up are dropped, never half-kept

	if (ncn && bringUpSm.systemStatusByte() != 0) {
		const KnxTransceiverProtocol::SystemStatus st =
			KnxTransceiverProtocol::decodeSystemStatus(bringUpSm.systemStatusByte());
		static const char* const MODES[] = { "power-up", "sync", "stop", "normal" };
		KnxDebug::log("DRV ncn5130 sysstat 0x%02X: v20v %s vdd2 %s vbus %s vfilt %s xtal %s%s, mode %s",
			(unsigned)bringUpSm.systemStatusByte(),
			st.v20v ? "ok" : "LOW", st.vdd2 ? "ok" : "LOW", st.vbus ? "ok" : "LOW",
			st.vfilt ? "ok" : "LOW", st.xtal ? "ok" : "BAD",
			st.thermalWarning ? ", THERMAL WARNING" : "", MODES[st.mode]);
	}
	if (ncn && bringUpSm.configureByte() != 0) {
		KnxDebug::log("DRV ncn5130 address %u.%u.%u confirmed, configure 0x%02X (auto-ack %s)",
			(unsigned)physicalAddress.area, (unsigned)physicalAddress.line,
			(unsigned)physicalAddress.device, (unsigned)bringUpSm.configureByte(),
			KnxTransceiverProtocol::autoAcknowledgeActive(bringUpSm.configureByte()) ? "on" : "OFF");
	}

	if (bringUpSm.succeeded()) {
		KnxDebug::log("DRV %s %s: ok, state 0x%02X", chipName, phase,
			(unsigned)bringUpSm.stateByte());
		return true;
	}

	const KnxBringUp::Step at = bringUpSm.failedAt();
	KnxDebug::log("DRV !! %s %s FAILED at %s (state 0x%02X)", chipName, phase, stepName(at),
		(unsigned)bringUpSm.stateByte());
	if (ncn && (at == KnxBringUp::Step::SystemStatus || at == KnxBringUp::Step::AwaitConfigure)) {
		KnxDebug::log("DRV    is this really an NCN5130? For a TP-UART2 pass KnxTransceiver::TpUart2");
	}
	if (at == KnxBringUp::Step::Reset) {
		KnxDebug::log("DRV    no reset answer: check wiring (RX/TX), bus voltage and the chip choice");
	}
	return false;
}

bool KnxDriver::awaitConfirmation(const uint8_t* frame, uint8_t length) {
	uint32_t start  = millis();
	uint8_t  echoed = 0;   // transmit-echo octets matched so far

	while (millis() - start < CON_TIMEOUT_MS) {
		if (!p_io->available()) continue;
		uint8_t b = p_io->read();

		// The TP-UART echoes every transmitted octet back before the L_Data.con. Those
		// octets must be consumed *before* the con test: a data byte may legally be 0x0B
		// or 0x8B, which isConfirmation() would otherwise read as the con — returning
		// early and leaking the rest of the echo into poll()/the reassembler.
		// Matched positionally rather than by count, so a front end that does not echo
		// (the ATTiny may not) still works: its con byte fails the match and falls through.
		if (echoed < length && b == frame[echoed]) {
			echoed++;
			continue;
		}

		if (KnxTransceiverProtocol::isConfirmation(b)) {
			if (echoed != 0 && echoed != length) {
				// Partial echo: the transceiver confirmed mid-echo, so the frame on the
				// bus may differ from what was handed to sendTelegram().
				KnxDebug::log("DRV !! echo %u/%u before con", (unsigned)echoed, (unsigned)length);
			} else if (echoed == length) {
				KnxDebug::log("DRV echo %u/%u ok", (unsigned)echoed, (unsigned)length);
			}
			// The con byte itself is logged: these are the spec-derived CON_* constants,
			// the least verified part of the driver, so show the raw byte behind the verdict.
			KnxDebug::log("DRV con byte 0x%02X -> %s", (unsigned)b,
				KnxTransceiverProtocol::isPositiveConfirmation(b) ? "positive" : "NEGATIVE");
			return KnxTransceiverProtocol::isPositiveConfirmation(b);
		}
		KnxDebug::log("DRV unexpected byte 0x%02X while awaiting con (echo %u/%u)",
			(unsigned)b, (unsigned)echoed, (unsigned)length);
	}
	KnxDebug::log("DRV !! no con within %u ms (echo %u/%u)",
		(unsigned)CON_TIMEOUT_MS, (unsigned)echoed, (unsigned)length);
	return false;   // no confirmation -> report failure, never a blind success
}

//---- IKnxDriver ----

bool KnxDriver::begin(void) {
	// Only when the driver was handed a HardwareSerial does it own the line settings. The
	// two-argument form is the one every core provides; on ESP32 it keeps whatever pins the
	// UART already has, so a caller who assigned their own pins beforehand keeps them.
	if (p_uart != nullptr) {
		p_uart->begin(BAUDRATE, SERIAL_8E1);
		KnxDebug::log("DRV port up @ %u baud 8E1", (unsigned)BAUDRATE);
	} else {
		KnxDebug::log("DRV using caller-configured stream");
	}
	// Plain INPUT, no MCU pull-up: the chip has its own to its 3.3 V (NCN5130) or VIF
	// (TP-UART2) rail, and a 5 V MCU pull-up would lift the line above the chip's supply.
	if (resetPin >= 0) pinMode(resetPin, INPUT);
	if (savePin  >= 0) pinMode(savePin,  INPUT);
	if (savePin  >= 0) lastSaveHigh = (digitalRead(savePin) == HIGH);

	if (!prepareChip()) return false;

	return bringUp("begin");
}

bool KnxDriver::reset(void) {
	if (!prepareChip()) return false;
	resetRoseLatch = false;   // our own TP-UART2 pulse is not a chip reset to react to
	return bringUp("reset");
}

bool KnxDriver::sendTelegram(const uint8_t* frame, uint8_t length) {
	if (chipInReset()) {
		KnxDebug::log("DRV !! not sent: chip in reset (reset pin low)");
		return false;
	}
	KnxDebug::logBytes("DRV tx frame:", frame, length);

	uint8_t pair[2];
	for (uint8_t i = 0; i < length; i++) {
		pair[0] = (uint8_t)((i == length - 1 ? KnxTransceiverProtocol::U_DATA_END
		                                   : KnxTransceiverProtocol::U_DATA_START_CONTINUE) | (i & 0x3F));
		pair[1] = frame[i];
		sendCommand(pair, sizeof(pair));
	}
	return awaitConfirmation(frame, length);
}

bool KnxDriver::poll(uint8_t* out, uint8_t maxLen, uint8_t& outLen) {
	if (!checkStatusPins()) return false;   // no UART traffic while the chip is in reset

	while (p_io->available()) {
		uint8_t b = p_io->read();
		// A Reset.ind outside a frame means the chip reset itself (bus voltage dip, watchdog)
		// and lost its address, so it would no longer acknowledge telegrams to us. 0x03 can
		// never start a frame, so the reassembler would only have dropped it.
		if (!reassembler.inFrame() && KnxTransceiverProtocol::isResetIndication(b)) {
			KnxDebug::log("DRV !! unexpected Reset.ind: chip reset itself, re-running bring-up");
			bringUp("recover");
			return false;
		}
		if (reassembler.feed(b)) {
			uint8_t n = reassembler.length();
			if (n > maxLen) {          // caller buffer too small — drop, stay in sync
				reassembler.reset();
				return false;
			}
			for (uint8_t i = 0; i < n; i++) out[i] = reassembler.frame()[i];
			outLen = n;
			return true;               // one frame per call; loop poll() to drain more
		}
	}
	return false;
}
