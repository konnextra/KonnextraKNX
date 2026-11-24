#pragma once
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name KnxFormat.h
 * @date 24.11.2026
 * @authors Florian Wiesner
 * @details Text rendering for the debug log (KnxLog::Basic / KnxLog::Deep): values, one-line
 *          telegram summaries and the Deep frame details. Arduino-free and host-tested. Never
 *          uses printf's %f (AVR's printf has no float support) and never writes past `max`:
 *          every function NUL-terminates and returns the number of characters written.
*/

//---- Standard / platform libraries ----
#include <stdint.h>

//---- Custom shared types ----
#include "KnxValue.h"
#include "KnxTelegramTypes.h"

namespace KnxFormat {

	// "DPT1: on", "DPT9: 21.50", "DPT10: Mon 13:05:09", ...; empty for an invalid value.
	uint8_t value(const KnxValue& value, char* out, uint8_t max);

	// "1.1.1 -> 0/1/1  Write     01", "1.1.1 -> 0/3/0  Read", "1.1.9 -> 1.1.5  Connect".
	uint8_t telegram(const ParsedTelegram& telegram, char* out, uint8_t max);

	// "ctrl BC  priority low  repeat no" for a raw standard frame.
	uint8_t frameControl(const uint8_t* raw, uint8_t len, char* out, uint8_t max);

	// "hops 6  length 1  checksum 36 ok" for a raw standard frame.
	uint8_t frameRouting(const uint8_t* raw, uint8_t len, char* out, uint8_t max);

	// Small bounded string builder shared by the functions above and the debug log.
	class Writer {
		private:
			char*   p_out;
			uint8_t max;
			uint8_t count = 0;

		public:
			Writer(char* out, uint8_t capacity) : p_out(out), max(capacity) {
				if (max > 0) p_out[0] = '\0';
			}
			void put(char c);
			void put(const char* text);
			void putUnsigned(uint32_t number, uint8_t minDigits = 1);
			void putSigned(int32_t number);
			void putHex2(uint8_t byte);
			void putFloat(float number);     // two decimals, or d.dde<exp> beyond 4e9
			void padTo(uint8_t column);      // spaces up to the given column
			uint8_t length(void) const { return count; }
	};

} // namespace KnxFormat
