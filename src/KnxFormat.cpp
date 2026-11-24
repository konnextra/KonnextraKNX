// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name KnxFormat.cpp
 * @date 24.11.2026
 * @authors Florian Wiesner
 * @details See KnxFormat.h. Bounded, printf-free text rendering for the debug log.
*/

//---- Libraries ----
#include "KnxFormat.h"
#include "KnxAddress.h"

namespace KnxFormat {

//---- Writer ----

void Writer::put(char c) {
	if (count + 1 >= maxLen) return;   // keep room for the terminator
	p_out[count++] = c;
	p_out[count] = '\0';
}

void Writer::put(const char* text) {
	while (*text) put(*text++);
}

void Writer::putUnsigned(uint32_t number, uint8_t minDigits) {
	char digits[10];
	uint8_t n = 0;
	do { digits[n++] = (char)('0' + number % 10); number /= 10; } while (number > 0);
	while (n < minDigits && n < sizeof(digits)) digits[n++] = '0';
	while (n > 0) put(digits[--n]);
}

void Writer::putSigned(int32_t number) {
	if (number < 0) { put('-'); putUnsigned((uint32_t)(-(number + 1)) + 1); }
	else            { putUnsigned((uint32_t)number); }
}

void Writer::putHex2(uint8_t byte) {
	static const char HEX_DIGITS[] = "0123456789ABCDEF";
	put(HEX_DIGITS[byte >> 4]);
	put(HEX_DIGITS[byte & 0x0F]);
}

void Writer::putFloat(float number) {
	if (number != number) { put("nan"); return; }
	if (number < 0) { put('-'); number = -number; }
	if (number > 3.4e38f) { put("inf"); return; }   // above FLT_MAX: only infinity
	if (number >= 4.0e9f) {
		// Too large for the 32-bit integer part: scientific notation, two decimals.
		int16_t exponent = 0;
		while (number >= 10.0f && exponent < 60) { number /= 10.0f; exponent++; }
		uint32_t hundredths = (uint32_t)(number * 100.0f + 0.5f);
		putUnsigned(hundredths / 100); put('.'); putUnsigned(hundredths % 100, 2);
		put('e'); putSigned(exponent);
		return;
	}
	uint32_t whole = (uint32_t)number;
	uint32_t hundredths = (uint32_t)((number - (float)whole) * 100.0f + 0.5f);
	if (hundredths >= 100) { whole++; hundredths -= 100; }
	putUnsigned(whole); put('.'); putUnsigned(hundredths, 2);
}

void Writer::padTo(uint8_t column) {
	while (count < column && count + 1 < maxLen) put(' ');
}

//---- Helpers ----

static void putPhysical(Writer& w, const PhysicalAddress& pa) {
	w.putUnsigned(pa.area); w.put('.'); w.putUnsigned(pa.line); w.put('.'); w.putUnsigned(pa.device);
}

static void putGroup(Writer& w, const GroupAddress& ga) {
	w.putUnsigned(ga.main); w.put('/'); w.putUnsigned(ga.middle); w.put('/'); w.putUnsigned(ga.sub);
}

static const char* tpciName(KnxTpci tpci) {
	switch (tpci) {
		case KnxTpci::Connect:        return "Connect";
		case KnxTpci::Disconnect:     return "Disconnect";
		case KnxTpci::Ack:            return "Ack";
		case KnxTpci::Nak:            return "Nak";
		case KnxTpci::DataIndividual: return "Data";
		case KnxTpci::DataConnected:  return "Data (connected)";
		default:                      return "?";
	}
}

static const char* WEEKDAYS[] = { "", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

static void putTime(Writer& w, uint8_t weekday, uint8_t h, uint8_t m, uint8_t s) {
	if (weekday >= 1 && weekday <= 7) { w.put(WEEKDAYS[weekday]); w.put(' '); }
	w.putUnsigned(h, 2); w.put(':'); w.putUnsigned(m, 2); w.put(':'); w.putUnsigned(s, 2);
}

static void putDate(Writer& w, uint16_t y, uint8_t mo, uint8_t d) {
	w.putUnsigned(y, 4); w.put('-'); w.putUnsigned(mo, 2); w.put('-'); w.putUnsigned(d, 2);
}

//---- Public functions ----

uint8_t value(const KnxValue& v, char* out, uint8_t maxLen) {
	Writer w(out, maxLen);
	if (!v.isValid()) return 0;

	w.put("DPT"); w.putUnsigned((uint8_t)v.dpt); w.put(": ");
	switch (v.dpt) {
		case KnxDpt::DPT1:  w.put(v.asBool() ? "on" : "off"); break;
		case KnxDpt::DPT2:
		case KnxDpt::DPT5:  w.putUnsigned(v.asU8()); break;
		case KnxDpt::DPT3: {
			DptDim d = v.asDim();
			if (d.stepcode == 0) w.put("stop");
			else { w.put(d.increase ? '+' : '-'); w.putUnsigned(d.stepcode); }
			break;
		}
		case KnxDpt::DPT4: {
			char c = v.asChar();
			if (c >= 0x20 && c < 0x7F) { w.put('\''); w.put(c); w.put('\''); }
			else                       { w.put("0x"); w.putHex2((uint8_t)c); }
			break;
		}
		case KnxDpt::DPT6:  w.putSigned(v.asI8()); break;
		case KnxDpt::DPT7:  w.putUnsigned(v.asU16()); break;
		case KnxDpt::DPT8:  w.putSigned(v.asI16()); break;
		case KnxDpt::DPT9:
		case KnxDpt::DPT14: w.putFloat(v.asFloat()); break;
		case KnxDpt::DPT10: { DptTime t = v.asTime(); putTime(w, t.weekday, t.hour, t.minute, t.second); break; }
		case KnxDpt::DPT11: { DptDate d = v.asDate(); putDate(w, d.year, d.month, d.day); break; }
		case KnxDpt::DPT12: w.putUnsigned(v.asU32()); break;
		case KnxDpt::DPT13: w.putSigned(v.asI32()); break;
		case KnxDpt::DPT19: {
			DptDateTime dt = v.asDateTime();
			putDate(w, dt.year, dt.month, dt.day); w.put(' ');
			putTime(w, 0, dt.hour, dt.minute, dt.second);
			break;
		}
		case KnxDpt::DPT232: {
			DptColor c = v.asColor();
			w.put('#'); w.putHex2(c.r); w.putHex2(c.g); w.putHex2(c.b);
			break;
		}
		default: break;
	}
	return w.length();
}

uint8_t telegram(const ParsedTelegram& t, char* out, uint8_t maxLen) {
	Writer w(out, maxLen);
	putPhysical(w, t.source);
	w.put(" -> ");

	if (t.addressType == KnxAddressType::Individual) {
		putPhysical(w, t.individualTarget);
		w.put("  ");
		if (t.hasApci) { w.put("APCI 0x"); w.putHex2((uint8_t)(t.apci >> 8)); w.putHex2((uint8_t)t.apci); }
		else           { w.put(tpciName(t.tpci)); }
		return w.length();
	}

	putGroup(w, t.target);
	w.put("  ");
	uint8_t typeStart = w.length();
	switch (t.type) {
		case GroupValueType::Read:     w.put("Read");     return w.length();   // carries no data
		case GroupValueType::Response: w.put("Response"); break;
		case GroupValueType::Write:    w.put("Write");    break;
		default:                       w.put("?");        break;
	}
	w.padTo((uint8_t)(typeStart + 10));   // data starts in the same column for every type

	if (t.payloadLength == 0) {
		w.putHex2(t.inline6Data);
	}
	else {
		for (uint8_t i = 0; i < t.payloadLength; i++) {
			if (i > 0) w.put(' ');
			w.putHex2(t.payload[i]);
		}
	}
	return w.length();
}

uint8_t frameControl(const uint8_t* raw, uint8_t len, char* out, uint8_t maxLen) {
	Writer w(out, maxLen);
	if (len < 1) return 0;
	static const char* PRIORITY[] = { "system", "normal", "urgent", "low" };
	w.put("ctrl "); w.putHex2(raw[0]);
	w.put("  priority "); w.put(PRIORITY[(raw[0] >> 2) & 0x03]);
	w.put("  repeat "); w.put((raw[0] & 0x20) ? "no" : "yes");
	return w.length();
}

uint8_t frameRouting(const uint8_t* raw, uint8_t len, char* out, uint8_t maxLen) {
	Writer w(out, maxLen);
	if (len < 7) return 0;
	w.put("hops "); w.putUnsigned((raw[5] >> 4) & 0x07);
	w.put("  length "); w.putUnsigned(raw[5] & 0x0F);
	uint8_t expected = 0xFF;
	for (uint8_t i = 0; i + 1 < len; i++) expected ^= raw[i];
	w.put("  checksum "); w.putHex2(raw[len - 1]);
	w.put(expected == raw[len - 1] ? " ok" : " BAD");
	return w.length();
}

} // namespace KnxFormat
