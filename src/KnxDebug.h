#pragma once
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name KnxDebug.h
 * @date 19.07.2026
 * @authors Florian Wiesner
 * @details Library-wide runtime logging with three levels (KnxLog::Off / Basic / Deep).
 *
 *          Off prints nothing at all. Basic prints one line per telegram sent or received,
 *          the start-up result, chip events, and every problem — problems marked with a
 *          leading "!" and a hint to switch to Deep. Deep prints each telegram as its own
 *          block: a separator line carrying the summary, then indented details (raw bytes,
 *          control field, hop count, checksum, driver traffic, per-listener results).
 *
 *          Line kinds, by who prints them:
 *            info()    Basic + Deep   "[knx] text"
 *            problem() Basic + Deep   "[knx] ! text" (+ hint at Basic)
 *            event()   Basic + Deep   info/problem at Basic, its own separator block at Deep
 *            section() Deep           "[knx] ----- text ------…" opens a telegram block
 *            banner()  Deep           "[knx] ===== text ======…" opens a start-up block
 *            log()     Deep           "[knx]   text" — an indented detail of the open block
 *
 *          The level is a single library-wide value in a function-local static, so the
 *          stateless namespaces (KnxFrame, KnxCodec) can log without an object to hang it on.
 *          This is a deliberate, agreed exception to the no-globals rule: it is a log level,
 *          not a service locator. Setting it on one object sets it for the whole library.
 *
 *          Lines go to a Print (default Serial) on Arduino. A host build formats them too and
 *          hands them to an optional line sink, which is how the native tests check the output;
 *          without a sink they are dropped. Arguments are evaluated even when the level filters
 *          the line out: guard expensive call sites with isEnabled() / isDeep().
*/

//---- Standard / platform libraries ----
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

//---- Custom shared types ----
#include "KnxEnums.h"   // KnxLog

namespace KnxDebug {

	//---- Config ----
	// Formatting buffer, stack-allocated per call — no dynamic allocation.
	static constexpr uint8_t LOG_BUFFER  = 112;
	// Separator lines are padded to this many characters after the "[knx] " prefix.
	static constexpr uint8_t SECTION_WIDTH = 64;

	typedef void (*LineSink)(const char* line);

	//---- Shared state (function-local statics: one instance across translation units) ----
	inline KnxLog& levelRef(void) { static KnxLog level = KnxLog::Off; return level; }
	inline LineSink& sinkRef(void) { static LineSink sink = nullptr; return sink; }
#ifdef ARDUINO
	inline Print*& outputRef(void) { static Print* output = &Serial; return output; }
#endif

	/** @brief The current log level. */
	inline KnxLog level(void) { return levelRef(); }

	/** @brief Sets the log level for the whole library. */
	inline void setLevel(KnxLog level) { levelRef() = level; }

	/** @brief True at Basic and Deep. Use to guard expensive call sites. */
	inline bool isEnabled(void) { return levelRef() != KnxLog::Off; }

	/** @brief True at Deep only. */
	inline bool isDeep(void) { return levelRef() == KnxLog::Deep; }

	/** @brief Legacy switch: true = Deep, false = Off. */
	inline void setEnabled(bool on) { levelRef() = on ? KnxLog::Deep : KnxLog::Off; }

	/** @brief Receives every finished line; used by host tests. nullptr = none. */
	inline void setLineSink(LineSink sink) { sinkRef() = sink; }

#ifdef ARDUINO
	/** @brief Where Arduino builds print; nullptr restores Serial. */
	inline void setOutput(Print* output) { outputRef() = output ? output : &Serial; }
#endif

	//---- Output ----
	// Hands one finished line (without newline) to the sink and, on Arduino, the output.
	inline void emit(const char* line) {
		if (sinkRef()) sinkRef()(line);
#ifdef ARDUINO
		outputRef()->println(line);
#endif
	}

	// Formats "[knx] " + lead + fmt into one buffer; fill > 0 pads with that char to
	// SECTION_WIDTH; withHint appends the "switch to Deep" hint.
	inline void vline(const char* lead, const char* fmt, va_list args, char fill, bool withHint) {
		static const char HINT[] = "  (more: setDebugMode(KnxLog::Deep))";
		char buffer[LOG_BUFFER];
		// The hint must survive a long line, so the text is cut short to leave room for it.
		const int limit = withHint ? (int)sizeof(buffer) - (int)sizeof(HINT) + 1 : (int)sizeof(buffer);
		int n = snprintf(buffer, (size_t)limit, "[knx] %s", lead);
		if (n < 0) return;
		if (n < limit) {
			int m = vsnprintf(buffer + n, (size_t)(limit - n), fmt, args);
			if (m > 0) n += m;
		}
		if (n >= limit) n = limit - 1;
		if (fill && n < (int)sizeof(buffer) - 2) {
			buffer[n++] = ' ';
			while (n < 6 + SECTION_WIDTH && n < (int)sizeof(buffer) - 1) buffer[n++] = fill;
			buffer[n] = '\0';
		}
		if (withHint) {
			snprintf(buffer + n, sizeof(buffer) - (size_t)n, "%s", HINT);
		}
		emit(buffer);
	}

	/** @brief A Basic-level line, e.g. one telegram summary. */
	inline void info(const char* fmt, ...) {
		if (!isEnabled()) return;
		va_list args; va_start(args, fmt);
		vline("", fmt, args, 0, false);
		va_end(args);
	}

	/** @brief Something went wrong: "! text", with the Deep hint at Basic. */
	inline void problem(const char* fmt, ...) {
		if (!isEnabled()) return;
		va_list args; va_start(args, fmt);
		vline("! ", fmt, args, 0, !isDeep());
		va_end(args);
	}

	/** @brief Opens a telegram block at Deep: "----- text -----…". */
	inline void section(const char* fmt, ...) {
		if (!isDeep()) return;
		va_list args; va_start(args, fmt);
		vline("----- ", fmt, args, '-', false);
		va_end(args);
	}

	/** @brief Opens a start-up block at Deep: "===== text =====…". */
	inline void banner(const char* fmt, ...) {
		if (!isDeep()) return;
		va_list args; va_start(args, fmt);
		vline("===== ", fmt, args, '=', false);
		va_end(args);
	}

	/** @brief A chip/bus event outside any telegram: its own block at Deep, one line at Basic. */
	inline void event(bool isProblem, const char* fmt, ...) {
		if (!isEnabled()) return;
		va_list args; va_start(args, fmt);
		if (isDeep())       vline(isProblem ? "----- ! " : "----- ", fmt, args, '-', false);
		else if (isProblem) vline("! ", fmt, args, 0, true);
		else                vline("", fmt, args, 0, false);
		va_end(args);
	}

	/** @brief An indented detail line of the open block; Deep only. */
	inline void log(const char* fmt, ...) {
		if (!isDeep()) return;
		va_list args; va_start(args, fmt);
		vline("  ", fmt, args, 0, false);
		va_end(args);
	}

	/** @brief An indented labelled hex dump; Deep only. Long dumps are cut at LOG_BUFFER. */
	inline void logBytes(const char* label, const uint8_t* data, uint8_t length) {
		if (!isDeep()) return;
		static const char HEX_DIGITS[] = "0123456789ABCDEF";
		char buffer[LOG_BUFFER];
		int n = snprintf(buffer, sizeof(buffer), "[knx]   %s", label);
		if (n < 0) return;
		for (uint8_t i = 0; i < length && n + 4 < (int)sizeof(buffer); i++) {
			buffer[n++] = ' ';
			buffer[n++] = HEX_DIGITS[data[i] >> 4];
			buffer[n++] = HEX_DIGITS[data[i] & 0x0F];
		}
		buffer[n] = '\0';
		emit(buffer);
	}

}   // namespace KnxDebug
