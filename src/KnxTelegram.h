#pragma once
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name KnxTelegram.h
 * @date 24.11.2026
 * @authors Florian Wiesner
 * @details The telegram handed to receive callbacks registered with subscribe() and
 *          onTelegram(): who sent it, to which group address, what kind of telegram it is,
 *          and its value. Arduino-free.
*/

//---- Standard / platform libraries ----
#include <stdint.h>

//---- Custom shared types + module headers ----
#include "KnxTelegramTypes.h"
#include "KnxAddress.h"
#include "KnxValue.h"
#include "KnxCodec.h"

/**
 * @brief A group telegram received from the bus, as passed to your receive callback.
 *        It is only valid while the callback runs: copy out what you need, such as value(),
 *        instead of keeping the telegram itself.
*/
class KnxTelegram {
	private:
		//---- Members ----
		const ParsedTelegram& parsed;   // parser output; its payload points into the RX buffer
		KnxValue decodedValue;          // value decoded with the subscription's datapoint type

	public:
		//---- Constructor ----
		/**
		 * @brief Wraps a received telegram. The library creates these for you.
		 * @param telegram The received telegram.
		 * @param value    The value already decoded from it, if any.
		*/
		explicit KnxTelegram(const ParsedTelegram& telegram, KnxValue value = KnxValue())
			: parsed(telegram), decodedValue(value) {}

		//---- Addressing ----
		/**
		 * @brief The group address the telegram was sent to.
		 * @return The group address in its packed 16-bit form.
		*/
		uint16_t groupAddress(void) const { return packGroupAddress(parsed.target); }

		/**
		 * @brief The device that sent the telegram.
		 * @return The sender's physical address.
		*/
		PhysicalAddress source(void) const { return parsed.source; }

		//---- Kind ----
		/**
		 * @brief The kind of telegram: a value written, a response to a read, or a read request.
		 * @return Write, Response or Read.
		*/
		GroupValueType type(void) const { return parsed.type; }

		/**
		 * @brief True if this is a read request, which carries no value.
		 * @return true for a read request.
		*/
		bool isRead(void) const { return parsed.type == GroupValueType::Read; }

		//---- Value ----
		/**
		 * @brief The value, already decoded with the datapoint type you subscribed with.
		 *        In an onTelegram() callback there is no datapoint type, so use decode() there.
		 * @return The value; check isValid() if you are not sure one was decoded.
		*/
		KnxValue value(void) const { return decodedValue; }

		/**
		 * @brief Decodes the telegram's value as the given datapoint type.
		 * @param dpt The datapoint type to read the value as.
		 * @return The value, or an invalid value for a read request or data that does not fit
		 *         the datapoint type.
		*/
		KnxValue decode(KnxDpt dpt) const {
			if (isRead()) return KnxValue();
			return KnxCodec::decodeTelegram(dpt, parsed);
		}

		//---- Raw data ----
		/**
		 * @brief The raw data bytes, for datapoint types the library does not decode. For the
		 *        small types (switching, dimming steps) this is the single 6-bit data byte.
		 * @return Pointer to the data bytes.
		*/
		const uint8_t* payload(void) const {
			return parsed.payloadLength == 0 ? &parsed.inline6Data : parsed.payload;
		}

		/**
		 * @brief Number of raw data bytes.
		 * @return The length of payload().
		*/
		uint8_t payloadLength(void) const {
			return parsed.payloadLength == 0 ? 1 : parsed.payloadLength;
		}
};

/** @brief A receive callback: `void myHandler(const KnxTelegram& telegram)`. */
typedef void (*KnxTelegramCallback)(const KnxTelegram& telegram);

/** @brief A receive callback with your own context pointer: `void myHandler(const KnxTelegram& telegram, void* context)`. */
typedef void (*KnxTelegramContextCallback)(const KnxTelegram& telegram, void* context);
