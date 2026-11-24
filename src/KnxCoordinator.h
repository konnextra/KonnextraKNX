#pragma once
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name KnxCoordinator.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details The KNX bus connection. It sends values to group addresses, receives telegrams and delivers
 *          them to the device objects attached to it, and exposes begin(), loop() and the debug
 *          switch a sketch calls on its `knx` object.
 *
 *          Most sketches never name this class directly: they create a `KonnextraKNX` (KonnextraKNX.h),
 *          which is a KnxCoordinator that also owns its bus driver, so the driver is set up for
 *          you. Use KnxCoordinator directly only for the advanced case of supplying your own
 *          driver (IKnxDriver) — for example a custom transceiver or a mock in a host test.
*/

//---- Standard / platform libraries ----
#include <stdint.h>
#include <stdlib.h>   // calloc for the one-time subscription table
#ifdef ARDUINO
#include <Arduino.h>
#endif

//---- Custom shared types + module headers ----
#include "KnxInterfaces.h"   // IKnxDriver, IKnxReceiver
#include "KnxAddress.h"
#include "KnxDebug.h"        // library-wide verbose logging switch
#include "KnxValue.h"
#include "KnxFrame.h"        // framing (pulls in KnxCodec)
#include "KnxTelegram.h"     // event type + callback typedefs for subscribe/onAnyTelegram
#include "KnxFormat.h"       // log line rendering

// Subscription slots allocated on the first subscribe() unless setSubscriptionSlotLimit() says
// otherwise. One slot is 12 bytes on 32-bit boards (64 = 768 B) and 8 bytes on AVR.
#define KNX_DEFAULT_SUBSCRIPTION_SLOTS 64

class KnxCoordinator {
	private:
		//---- Config ----
		// RX buffer holds a full standard frame (up to 6 header + 16 APDU + checksum).
		static constexpr uint8_t RX_BUFFER_SIZE = 24;

		//---- Members ----
		IKnxDriver*    p_driver;                 // injected link-layer driver
		PhysicalAddress physicalAddress;         // frame source address
		IKnxReceiver*  receiverHead = nullptr;   // intrusive registry head
		IKnxDeviceHandler* p_deviceHandler = nullptr;   // optional point-to-point handler
		uint8_t        rxFrame[RX_BUFFER_SIZE];

		// One receive callback in either accepted shape; hasContext says which member is live.
		union TelegramHandler {
			KnxTelegramCallback        plain;
			KnxTelegramContextCallback withContext;
		};
		TelegramHandler telegramHook = { nullptr };   // onAnyTelegram() slot
		bool  _hookHasContext = false;
		void* p_hookContext   = nullptr;

		// One subscribe() entry. flags: SLOT_USED, SLOT_HAS_CONTEXT.
		struct Subscription {
			uint16_t        groupAddress;
			KnxDpt          dpt;
			uint8_t         flags;
			TelegramHandler handler;
			void*           context;
		};
		static constexpr uint8_t SLOT_USED        = 0x01;
		static constexpr uint8_t SLOT_HAS_CONTEXT = 0x02;

		Subscription* p_subscriptions = nullptr;   // allocated once, on the first subscribe()
		uint16_t subscriptionLimit = KNX_DEFAULT_SUBSCRIPTION_SLOTS;
		bool _inLoop = false;   // guards loop() against being called from a callback

		//---- Methods ----
		// Walks the receiver registry and delivers a parsed telegram to every match.
		void dispatch(const ParsedTelegram& telegram);

		// Routes a telegram addressed to an individual address: ours goes to the device
		// handler, anyone else's is foreign traffic.
		void dispatchIndividual(const ParsedTelegram& telegram);

		// Stores a subscription in the first free slot, allocating the table on first use.
		bool addSubscription(uint16_t groupAddress, KnxDpt dpt, TelegramHandler handler,
		                     uint8_t flags, void* context);

		// Sends a built frame and logs the verdict (Basic line or Deep block close).
		bool transmit(const uint8_t* frame, uint8_t len, const KnxValue* value);

		// Writes the Basic line or opens the Deep block for a received telegram, before dispatch.
		void logReceived(const ParsedTelegram& telegram, uint8_t len);

		// Listeners a group telegram will reach, and the first known DPT for showing its value.
		uint16_t countListeners(const ParsedTelegram& telegram, KnxDpt& displayDpt) const;

		// Delivers a Write/Response to every matching subscription; returns how many fired.
		uint16_t dispatchSubscriptions(const ParsedTelegram& telegram, uint16_t groupAddress);

	public:
		//---- Constructor ----
		/**
		 * @brief Advanced: creates a bus connection driven by a bus driver you provide. Most sketches
		 *        use KonnextraKNX instead, which supplies the driver for you.
		 * @param driver          The bus driver to use; not owned, and must outlive this object.
		 * @param physicalAddress This device's physical address.
		*/
		KnxCoordinator(IKnxDriver* driver, PhysicalAddress physicalAddress)
			: p_driver(driver), physicalAddress(physicalAddress) {}

		//---- Lifecycle ----
		/**
		 * @brief Starts the bus connection. Call once in setup().
		 * @return true if the bus came up successfully.
		*/
		bool begin(void) { return p_driver->begin(); }

		/**
		 * @brief Restarts the bus connection.
		 * @return true if the reset succeeded.
		*/
		bool reset(void) { return p_driver->reset(); }

		//---- Sending ----
		/**
		 * @brief Sends a value to a group address.
		 * @param groupAddress Destination group address.
		 * @param value        The value to send.
		 * @return true if the bus confirmed the send.
		*/
		bool send(uint16_t groupAddress, const KnxValue& value);

		//---- Receiving ----
		/**
		 * @brief Attaches a device object so it receives matching telegrams. Objects do this
		 *        themselves when created, so you rarely call it directly.
		 * @param receiver The object to attach; not owned, and must outlive this object.
		*/
		void registerReceiver(IKnxReceiver* receiver);

		/**
		 * @brief Detaches a device object (safe if it was not attached). Objects do this
		 *        themselves when destroyed.
		 * @param receiver The object to detach.
		*/
		void unregisterReceiver(IKnxReceiver* receiver);

		/**
		 * @brief Advanced: installs a handler for telegrams sent to this device by its physical
		 *        address (device management, such as a ping from ETS). Without one, such
		 *        telegrams are ignored.
		 * @param handler The handler to install, or nullptr to remove it; not owned, and must
		 *                outlive this object.
		*/
		void setDeviceHandler(IKnxDeviceHandler* handler) { p_deviceHandler = handler; }

		/**
		 * @brief Calls a function whenever a value arrives on a group address — the receiving
		 *        counterpart to send(), with no device object needed. It fires for values written
		 *        to the address and for responses to read requests, never for the read requests
		 *        themselves. Several functions may listen on the same address.
		 * @param groupAddress Group address to listen on.
		 * @param dpt          Datapoint type to decode the value as.
		 * @param callback     Called with each telegram; its value() is already decoded.
		 * @return true if the subscription was stored; false if callback is nullptr, all slots
		 *         are in use, or there was no memory for the slot table.
		*/
		bool subscribe(uint16_t groupAddress, KnxDpt dpt, KnxTelegramCallback callback) {
			TelegramHandler handler;
			handler.plain = callback;
			return addSubscription(groupAddress, dpt, handler, 0, nullptr);
		}

		/**
		 * @brief Like subscribe(groupAddress, dpt, callback), but also hands your own pointer back
		 *        to the callback, for code that needs to know which of its objects a value is for.
		 * @param groupAddress Group address to listen on.
		 * @param dpt          Datapoint type to decode the value as.
		 * @param callback     Called with each telegram and the context pointer.
		 * @param context      Any pointer of yours; passed through unchanged.
		 * @return true if the subscription was stored.
		*/
		bool subscribe(uint16_t groupAddress, KnxDpt dpt, KnxTelegramContextCallback callback,
		               void* context) {
			TelegramHandler handler;
			handler.withContext = callback;
			return addSubscription(groupAddress, dpt, handler, SLOT_HAS_CONTEXT, context);
		}

		/**
		 * @brief Stops every subscription on a group address and frees their slots. Safe to call
		 *        from inside a callback; it takes effect immediately.
		 * @param groupAddress Group address to stop listening on.
		*/
		void unsubscribe(uint16_t groupAddress);

		/**
		 * @brief Sets how many subscriptions this connection can hold in total (default 64).
		 *        Call it in setup() before the first subscribe(): the slots are reserved once, at
		 *        the first subscribe(), and cannot be resized afterwards.
		 * @param total Number of subscription slots.
		 * @return true if the limit was set; false if subscribe() has already been called or
		 *         the board cannot address that much memory.
		*/
		bool setSubscriptionSlotLimit(uint16_t total);

		/**
		 * @brief Calls a function for every group telegram on the bus — including telegrams for
		 *        other devices and read requests — before any subscription or device object sees
		 *        it. Meant for bus monitors and for integrations that keep their own list of
		 *        addresses. There is one such function: a new call replaces the previous one.
		 * @param callback Called with each telegram; decode its value with KnxTelegram::decode().
		 *                 Pass nullptr to remove it.
		*/
		void onAnyTelegram(KnxTelegramCallback callback) {
			telegramHook.plain = callback;
			_hookHasContext = false;
			p_hookContext = nullptr;
		}

		/**
		 * @brief Like onAnyTelegram(callback), but also hands your own pointer back to the callback,
		 *        for code that needs to know which of its objects a telegram belongs to.
		 * @param callback Called with each telegram and the context pointer.
		 * @param context  Any pointer of yours; passed through unchanged.
		*/
		void onAnyTelegram(KnxTelegramContextCallback callback, void* context) {
			telegramHook.withContext = callback;
			_hookHasContext = (callback != nullptr);
			p_hookContext = context;
		}

		//---- Point-to-point sending (advanced) ----
		/**
		 * @brief Advanced: sends a message to a single device addressed by its physical address,
		 *        for example to ping it.
		 * @param target        Physical address of the device to send to.
		 * @param apci          Application service code to send.
		 * @param payload       Optional data after the service code (may be nullptr).
		 * @param payloadLength Number of payload bytes.
		 * @return true if the bus confirmed the send.
		*/
		bool sendIndividual(PhysicalAddress target, uint16_t apci,
		                    const uint8_t* payload = nullptr, uint8_t payloadLength = 0);

		/**
		 * @brief Advanced: sends a transport control message (connect, disconnect, acknowledge)
		 *        to a single device addressed by its physical address.
		 * @param target         Physical address of the device to send to.
		 * @param tpci           The control message to send.
		 * @param sequenceNumber Sequence number, used only for acknowledge messages.
		 * @return true if the bus confirmed the send.
		*/
		bool sendControl(PhysicalAddress target, KnxTpci tpci, uint8_t sequenceNumber = 0);

		/**
		 * @brief Services the bus: call this on every iteration of loop(). It receives waiting
		 *        telegrams and fires the callbacks of the objects they are addressed to. It never
		 *        blocks, so do not replace it with delay()-based timing. Calling it from inside a
		 *        receive callback does nothing.
		 * @return true if at least one telegram was handled this call.
		*/
		bool loop(void);

		/**
		 * @brief This device's physical address.
		 * @return The physical address this connection was created with.
		*/
		PhysicalAddress address(void) const { return physicalAddress; }

		//---- Diagnostics ----
		/**
		 * @brief Sets how much the library prints about its work. Off (the default) prints
		 *        nothing. Basic prints one line per telegram sent or received, the start-up
		 *        result, and problems marked with "!". Deep prints each telegram as its own block
		 *        with all its details, plus every start-up step — useful to diagnose a problem,
		 *        but chatty enough to slow the receive path. Call it before begin() to see start-up.
		 * @param level KnxLog::Off, KnxLog::Basic or KnxLog::Deep.
		 * @note  This affects the whole library, not just this connection.
		*/
		void setDebugMode(KnxLog level) { KnxDebug::setLevel(level); }

		/**
		 * @brief Shorthand for setDebugMode(): true is KnxLog::Deep, false is KnxLog::Off.
		 * @param on true to print everything, false to print nothing.
		*/
		void enableDebugMode(bool on) { KnxDebug::setEnabled(on); }

// ================= Arduino String conveniences =================
#ifdef ARDUINO
	public:
		/**
		 * @brief Sends the debug output to another port instead of Serial — a second UART,
		 *        a Bluetooth serial, or anything else that can print.
		 * @param port The port to print to, e.g. &Serial2. nullptr goes back to Serial.
		*/
		void setDebugOutputPort(Print* port) { KnxDebug::setOutput(port); }

		/**
		 * @brief Advanced: creates a bus connection from a driver you provide and an address string.
		 * @param driver          The bus driver to use; not owned, and must outlive this object.
		 * @param physicalAddress This device's physical address as "area.line.device", e.g. "1.1.5".
		*/
		KnxCoordinator(IKnxDriver* driver, String physicalAddress)
			: p_driver(driver), physicalAddress(physicalAddressFromString(physicalAddress)) {}

		/**
		 * @brief Sends a value to a group address given as a string, without needing a device
		 *        object. To receive the same way, use subscribe().
		 * @param ga    Destination group address as "main/middle/sub", e.g. "0/4/2".
		 * @param value The value to send.
		 * @return true if the bus confirmed the send.
		*/
		bool send(String ga, const KnxValue& value) { return send(packedGroupAddressFromString(ga), value); }

		/**
		 * @brief Calls a function whenever a value arrives on a group address given as a string —
		 *        the receiving counterpart to send(), with no device object needed.
		 * @param ga       Group address as "main/middle/sub", e.g. "0/3/0".
		 * @param dpt      Datapoint type to decode the value as.
		 * @param callback Called with each telegram; its value() is already decoded.
		 * @return true if the subscription was stored.
		*/
		bool subscribe(String ga, KnxDpt dpt, KnxTelegramCallback callback) {
			return subscribe(packedGroupAddressFromString(ga), dpt, callback);
		}

		/**
		 * @brief Like subscribe(ga, dpt, callback), but also hands your own pointer back to the
		 *        callback.
		 * @param ga       Group address as "main/middle/sub".
		 * @param dpt      Datapoint type to decode the value as.
		 * @param callback Called with each telegram and the context pointer.
		 * @param context  Any pointer of yours; passed through unchanged.
		 * @return true if the subscription was stored.
		*/
		bool subscribe(String ga, KnxDpt dpt, KnxTelegramContextCallback callback, void* context) {
			return subscribe(packedGroupAddressFromString(ga), dpt, callback, context);
		}

		/**
		 * @brief Stops every subscription on a group address given as a string.
		 * @param ga Group address as "main/middle/sub".
		*/
		void unsubscribe(String ga) { unsubscribe(packedGroupAddressFromString(ga)); }
#endif // ARDUINO
};
