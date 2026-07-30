/**
 * @name KnxDriver.cpp
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details See KnxDriver.h. Adapted from the thesis KNX_TPUART2 driver; the telegram-layer
 *          coupling is removed (RX is exposed as reassembled frames via poll()) and the send
 *          path now reads the real L_Data.con instead of hard-returning true (PLAN §9).
*/

//----Libraries----
#include "KnxDriver.h"

#ifdef KNX_DEFAULT_PORT
KnxDriver::KnxDriver(String physicalAddress)
	: KnxDriver(physicalAddress, KNX_DEFAULT_PORT) {}
#endif

// The port is remembered twice on purpose: as a Stream for the byte traffic, and as a
// HardwareSerial so begin() knows it may configure the line.
KnxDriver::KnxDriver(String physicalAddress, HardwareSerial& port)
	: p_io(&port), p_uart(&port),
	  physicalAddress(physicalAddressFromString(physicalAddress)) {}

// Stream path: the caller owns the line settings, so p_uart stays null.
KnxDriver::KnxDriver(String physicalAddress, Stream& stream)
	: p_io(&stream), p_uart(nullptr),
	  physicalAddress(physicalAddressFromString(physicalAddress)) {}

//---- Private methods ----

void KnxDriver::sendCommand(const uint8_t* cmd, uint16_t len) {
	p_io->write(cmd, len);
}

void KnxDriver::clearBuffer(void) {
	while (p_io->available()) (void)p_io->read();
}

bool KnxDriver::resetRequest(void) {
	// Copied into a local first: taking the address of a static constexpr member ODR-uses
	// it, which needs an out-of-line definition before C++17. The copy keeps the driver
	// buildable on toolchains that still default to C++11, as AVR does.
	const uint8_t cmd = U_RESET_REQ;
	sendCommand(&cmd, sizeof(cmd));
	clearBuffer();
	delay(RESPONSE_TIME_MS);

	// Wait for Reset.indication.
	uint32_t start = millis();
	while (millis() - start < RESPONSE_TIME_MS) {
		if (p_io->available()) {
			uint8_t b = p_io->read();
			if (b != 0x00 && b == U_RESET_IND) return true;   // ignore RX-idle 0x00
		}
	}

	// No hardware fallback: the front end no longer exposes a /RESET line, so a soft reset
	// that goes unanswered is simply a failure to report.
	return false;
}

uint8_t KnxDriver::stateRequest(void) {
	const uint8_t cmd = U_STATE_REQ;   // see resetRequest() for why this is copied
	sendCommand(&cmd, sizeof(cmd));
	delay(RESPONSE_TIME_MS);

	uint32_t start = millis();
	while (millis() - start < RESPONSE_TIME_MS) {
		if (p_io->available()) return p_io->read();
	}
	return 0xFF;
}

void KnxDriver::applyPhysicalAddress(void) {
	uint8_t addressHigh = (physicalAddress.area << 4) | physicalAddress.line;
	uint8_t addressLow  = physicalAddress.device;
	uint8_t cmd[3]      = { U_SET_ADDRESS, addressHigh, addressLow };
	sendCommand(cmd, sizeof(cmd));
}

bool KnxDriver::isConfirmation(uint8_t b) {
	return (b & CON_MASK) == CON_PATTERN;
}

bool KnxDriver::isPositiveConfirmation(uint8_t b) {
	return (b & CON_POSITIVE) != 0;
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

		if (isConfirmation(b)) {
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
				isPositiveConfirmation(b) ? "positive" : "NEGATIVE");
			return isPositiveConfirmation(b);
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

	bool reset_ok = resetRequest();
	applyPhysicalAddress();
	uint8_t state = stateRequest();
	KnxDebug::log("DRV begin: reset %s, state 0x%02X",
		reset_ok ? "ok" : "FAILED", (unsigned)state);
	return reset_ok && (state == U_STATE_IND_OK);
}

bool KnxDriver::reset(void) {
	bool reset_ok = resetRequest();
	applyPhysicalAddress();
	uint8_t state = stateRequest();
	KnxDebug::log("DRV reset: %s, state 0x%02X",
		reset_ok ? "ok" : "FAILED", (unsigned)state);
	return reset_ok && (state == U_STATE_IND_OK);
}

bool KnxDriver::sendTelegram(const uint8_t* frame, uint8_t length) {
	KnxDebug::logBytes("DRV tx frame:", frame, length);

	uint8_t pair[2];
	for (uint8_t i = 0; i < length; i++) {
		pair[0] = (uint8_t)((i == length - 1 ? U_DATA_END : U_DATA_START_CONTINUE) | (i & 0x3F));
		pair[1] = frame[i];
		sendCommand(pair, sizeof(pair));
	}
	return awaitConfirmation(frame, length);
}

bool KnxDriver::poll(uint8_t* out, uint8_t maxLen, uint8_t& outLen) {
	while (p_io->available()) {
		uint8_t b = p_io->read();
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
