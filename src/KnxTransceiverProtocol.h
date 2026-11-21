#pragma once
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name KnxTransceiverProtocol.h
 * @date 21.11.2026
 * @authors Florian Wiesner
 * @details The UART service bytes spoken between the host and a KNX bus coupler chip, for
 *          every chip KnxTransceiver names. Stateless and Arduino-free, like KnxFrame, so the
 *          byte values and classifiers are host-tested.
 *
 *          The two chips share almost the whole protocol — reset, state, the data-transfer
 *          encoding, the echo and L_Data.con. They differ in setting the physical address:
 *          the TP-UART2 takes `0x28 hi lo`, the NCN5130 `0xF1 hi lo dummy`. On the NCN5130
 *          `0x28` is a write to its internal watchdog register, after which the address low
 *          byte runs as a command of its own — for x.x.5 that is `0x05`, U_Busmon.req.
*/

//---- Libraries ----
#include <stdint.h>

//---- Custom shared types ----
#include "KnxEnums.h"
#include "KnxAddress.h"

namespace KnxTransceiverProtocol {
	//---- Host -> chip, common ----
	constexpr uint8_t U_RESET_REQ           = 0x01;
	constexpr uint8_t U_STATE_REQ           = 0x02;
	constexpr uint8_t U_DATA_START_CONTINUE = 0x80;
	constexpr uint8_t U_DATA_END            = 0x40;

	//---- Host -> chip, chip-specific ----
	constexpr uint8_t TPUART2_SET_ADDRESS   = 0x28;   // + hi, lo
	constexpr uint8_t NCN5130_SET_ADDRESS   = 0xF1;   // + hi, lo, dummy
	constexpr uint8_t NCN5130_SYSTEMSTAT    = 0x0D;   // answered by 0x4B + status byte

	//---- Chip -> host ----
	constexpr uint8_t U_RESET_IND            = 0x03;
	constexpr uint8_t U_STATE_IND_OK         = 0x07;   // state indication with no error flag
	constexpr uint8_t NCN5130_SYSTEMSTAT_IND = 0x4B;

	// Longest set-address command over all chips (NCN5130: 4 bytes).
	constexpr uint8_t SET_ADDRESS_MAX_LEN   = 4;

	// NCN5130 operation modes, bits 1..0 of the U_SystemStat.ind status byte.
	constexpr uint8_t MODE_POWER_UP = 0;
	constexpr uint8_t MODE_SYNC     = 1;
	constexpr uint8_t MODE_STOP     = 2;
	constexpr uint8_t MODE_NORMAL   = 3;

	// The NCN5130 U_SystemStat.ind status byte, decoded (datasheet Figure 38).
	struct SystemStatus {
		bool    v20v;             // 20 V regulator in range
		bool    vdd2;             // DC2 regulator in range
		bool    vbus;             // bus voltage in range
		bool    vfilt;            // tank capacitor voltage in range
		bool    xtal;             // crystal oscillator in range
		bool    thermalWarning;   // set when the chip is too hot
		uint8_t mode;             // MODE_POWER_UP .. MODE_NORMAL
	};

	// Writes the set-address command for chip into out (SET_ADDRESS_MAX_LEN bytes) and
	// returns its length: 3 for the TP-UART2, 4 for the NCN5130.
	uint8_t buildSetAddress(KnxTransceiver chip, PhysicalAddress address, uint8_t* out);

	// Bring-up timing per chip. The NCN5130 answers a reset only after 40 bit times of
	// bus silence, so it gets a far wider window than the TP-UART2.
	uint16_t resetTimeoutMs(KnxTransceiver chip);
	// True if the chip confirms a new address with U_Configure.ind (NCN5130 only).
	bool confirmsAddress(KnxTransceiver chip);
	// How long to wait for that confirmation; it is sent once the bus is idle.
	uint16_t addressConfirmTimeoutMs(KnxTransceiver chip);
	// True if the chip answers U_SystemStat.req (NCN5130 only).
	bool hasSystemStatus(KnxTransceiver chip);

	//---- Byte classification, chip-independent ----
	bool isResetIndication(uint8_t b);
	bool isStateIndication(uint8_t b);           // (b & 0x07) == 0x07
	bool isConfirmation(uint8_t b);              // (b & 0x7F) == 0x0B
	bool isPositiveConfirmation(uint8_t b);      // bit 7 set
	bool isConfigureIndication(uint8_t b);       // (b & 0x83) == 0x01
	bool autoAcknowledgeActive(uint8_t configureInd);   // aa flag, bit 5

	// Decodes the second byte of an NCN5130 U_SystemStat.ind.
	SystemStatus decodeSystemStatus(uint8_t statusByte);
}
