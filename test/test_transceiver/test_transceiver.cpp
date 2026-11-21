/**
 * @name test_transceiver.cpp
 * @date 21.11.2026
 * @authors Florian Wiesner
 * @details Host unit tests for KnxTransceiverProtocol: set-address bytes per chip (incl. the
 *          NCN5130 busmonitor trap), byte classifiers, system status decoding and timing.
 *          Run with: pio test -e native
*/

#include <unity.h>
#include "KnxTransceiverProtocol.h"

using namespace KnxTransceiverProtocol;

void setUp(void) {}
void tearDown(void) {}

static PhysicalAddress pa(uint8_t a, uint8_t l, uint8_t d) {
	PhysicalAddress p = { a, l, d };
	return p;
}

void test_set_address_tpuart2(void) {
	uint8_t out[SET_ADDRESS_MAX_LEN] = { 0 };
	TEST_ASSERT_EQUAL_UINT8(3, buildSetAddress(KnxTransceiver::TpUart2, pa(1, 1, 5), out));
	const uint8_t expected[] = { 0x28, 0x11, 0x05 };
	TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, 3);
}

void test_set_address_ncn5130(void) {
	uint8_t out[SET_ADDRESS_MAX_LEN] = { 0 };
	TEST_ASSERT_EQUAL_UINT8(4, buildSetAddress(KnxTransceiver::Ncn5130, pa(1, 1, 5), out));
	const uint8_t expected[] = { 0xF1, 0x11, 0x05, 0x00 };
	TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, 4);
}

// 0x28 on an NCN5130 writes the watchdog register and runs the low byte as a command —
// 0x05 (x.x.5) is U_Busmon.req. It must never be sent to that chip.
void test_ncn5130_never_sends_tpuart2_opcode(void) {
	const PhysicalAddress addresses[] = { pa(1, 1, 1), pa(1, 1, 5), pa(15, 15, 255), pa(0, 0, 0) };
	for (uint8_t i = 0; i < 4; i++) {
		uint8_t out[SET_ADDRESS_MAX_LEN] = { 0 };
		buildSetAddress(KnxTransceiver::Ncn5130, addresses[i], out);
		TEST_ASSERT_NOT_EQUAL(0x28, out[0]);
	}
}

void test_set_address_upper_bounds(void) {
	uint8_t out[SET_ADDRESS_MAX_LEN] = { 0 };
	buildSetAddress(KnxTransceiver::TpUart2, pa(15, 15, 255), out);
	TEST_ASSERT_EQUAL_HEX8(0xFF, out[1]);
	TEST_ASSERT_EQUAL_HEX8(0xFF, out[2]);
	buildSetAddress(KnxTransceiver::Ncn5130, pa(15, 15, 255), out);
	TEST_ASSERT_EQUAL_HEX8(0xFF, out[1]);
	TEST_ASSERT_EQUAL_HEX8(0xFF, out[2]);
}

void test_configure_indication(void) {
	TEST_ASSERT_TRUE(isConfigureIndication(0x21));
	TEST_ASSERT_TRUE(isConfigureIndication(0x01));
	const uint8_t no[] = { 0x03, 0x07, 0x0B, 0x8B, 0xBC };
	for (uint8_t i = 0; i < sizeof(no); i++) TEST_ASSERT_FALSE(isConfigureIndication(no[i]));
	TEST_ASSERT_TRUE(autoAcknowledgeActive(0x21));
	TEST_ASSERT_FALSE(autoAcknowledgeActive(0x01));
}

void test_confirmation_and_state(void) {
	TEST_ASSERT_TRUE(isConfirmation(0x8B));
	TEST_ASSERT_TRUE(isConfirmation(0x0B));
	TEST_ASSERT_FALSE(isConfirmation(0x21));
	TEST_ASSERT_FALSE(isConfirmation(0x13));
	TEST_ASSERT_TRUE(isPositiveConfirmation(0x8B));
	TEST_ASSERT_FALSE(isPositiveConfirmation(0x0B));
	TEST_ASSERT_TRUE(isStateIndication(0x07));
	TEST_ASSERT_TRUE(isStateIndication(0x0F));
	TEST_ASSERT_FALSE(isStateIndication(0x03));
	TEST_ASSERT_TRUE(isResetIndication(0x03));
}

void test_answers_never_look_like_a_control_byte(void) {
	// KnxBringUp relies on this: no chip answer may start an L_Data_Standard frame.
	const uint8_t answers[] = { 0x03, 0x07, 0x21, 0x01, 0x4B, 0x8B, 0x0B };
	for (uint8_t i = 0; i < sizeof(answers); i++) {
		TEST_ASSERT_NOT_EQUAL(0x90, answers[i] & 0xD3);
	}
}

void test_decode_system_status(void) {
	SystemStatus s = decodeSystemStatus(0xFB);   // all rails ok, no TW, Normal
	TEST_ASSERT_TRUE(s.v20v && s.vdd2 && s.vbus && s.vfilt && s.xtal);
	TEST_ASSERT_FALSE(s.thermalWarning);
	TEST_ASSERT_EQUAL_UINT8(MODE_NORMAL, s.mode);

	TEST_ASSERT_TRUE(decodeSystemStatus(0x80).v20v);
	TEST_ASSERT_TRUE(decodeSystemStatus(0x40).vdd2);
	TEST_ASSERT_TRUE(decodeSystemStatus(0x20).vbus);
	TEST_ASSERT_TRUE(decodeSystemStatus(0x10).vfilt);
	TEST_ASSERT_TRUE(decodeSystemStatus(0x08).xtal);
	TEST_ASSERT_TRUE(decodeSystemStatus(0x04).thermalWarning);
	TEST_ASSERT_FALSE(decodeSystemStatus(0x7F).v20v);
	for (uint8_t m = 0; m < 4; m++) TEST_ASSERT_EQUAL_UINT8(m, decodeSystemStatus(m).mode);
}

void test_timing(void) {
	TEST_ASSERT_TRUE(resetTimeoutMs(KnxTransceiver::Ncn5130) >= 100);
	TEST_ASSERT_EQUAL_UINT16(20, resetTimeoutMs(KnxTransceiver::TpUart2));
	TEST_ASSERT_TRUE(confirmsAddress(KnxTransceiver::Ncn5130));
	TEST_ASSERT_FALSE(confirmsAddress(KnxTransceiver::TpUart2));
	TEST_ASSERT_FALSE(hasSystemStatus(KnxTransceiver::TpUart2));
}

int main(int, char**) {
	UNITY_BEGIN();
	RUN_TEST(test_set_address_tpuart2);
	RUN_TEST(test_set_address_ncn5130);
	RUN_TEST(test_ncn5130_never_sends_tpuart2_opcode);
	RUN_TEST(test_set_address_upper_bounds);
	RUN_TEST(test_configure_indication);
	RUN_TEST(test_confirmation_and_state);
	RUN_TEST(test_answers_never_look_like_a_control_byte);
	RUN_TEST(test_decode_system_status);
	RUN_TEST(test_timing);
	return UNITY_END();
}
