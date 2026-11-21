// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name KnxTransceiverProtocol.cpp
 * @date 21.11.2026
 * @authors Florian Wiesner
 * @details See KnxTransceiverProtocol.h.
*/

//----Libraries----
#include "KnxTransceiverProtocol.h"

namespace KnxTransceiverProtocol {

uint8_t buildSetAddress(KnxTransceiver chip, PhysicalAddress address, uint8_t* out) {
	const uint8_t hi = (uint8_t)(((address.area & 0x0F) << 4) | (address.line & 0x0F));
	const uint8_t lo = address.device;
	if (chip == KnxTransceiver::TpUart2) {
		out[0] = TPUART2_SET_ADDRESS;
		out[1] = hi;
		out[2] = lo;
		return 3;
	}
	out[0] = NCN5130_SET_ADDRESS;
	out[1] = hi;
	out[2] = lo;
	out[3] = 0x00;   // dummy byte the NCN5130 expects after the address
	return 4;
}

uint16_t resetTimeoutMs(KnxTransceiver chip) {
	// NCN5130: ~8 ms reset, then at least 40 Tbit of silence before U_Reset.ind,
	// plus headroom for a busy bus. TP-UART2: the window verified on hardware.
	return chip == KnxTransceiver::Ncn5130 ? 200 : 20;
}

bool confirmsAddress(KnxTransceiver chip) {
	return chip == KnxTransceiver::Ncn5130;
}

uint16_t addressConfirmTimeoutMs(KnxTransceiver chip) {
	return chip == KnxTransceiver::Ncn5130 ? 100 : 0;
}

bool hasSystemStatus(KnxTransceiver chip) {
	return chip == KnxTransceiver::Ncn5130;
}

bool isResetIndication(uint8_t b)      { return b == U_RESET_IND; }
bool isStateIndication(uint8_t b)      { return (b & 0x07) == 0x07; }
bool isConfirmation(uint8_t b)         { return (b & 0x7F) == 0x0B; }
bool isPositiveConfirmation(uint8_t b) { return (b & 0x80) != 0; }
bool isConfigureIndication(uint8_t b)  { return (b & 0x83) == 0x01; }
bool autoAcknowledgeActive(uint8_t configureInd) { return (configureInd & 0x20) != 0; }

SystemStatus decodeSystemStatus(uint8_t statusByte) {
	SystemStatus s;
	s.v20v           = (statusByte & 0x80) != 0;
	s.vdd2           = (statusByte & 0x40) != 0;
	s.vbus           = (statusByte & 0x20) != 0;
	s.vfilt          = (statusByte & 0x10) != 0;
	s.xtal           = (statusByte & 0x08) != 0;
	s.thermalWarning = (statusByte & 0x04) != 0;
	s.mode           = statusByte & 0x03;
	return s;
}

}
