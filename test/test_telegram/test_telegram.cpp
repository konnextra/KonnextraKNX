/**
 * @name test_telegram.cpp
 * @date 24.11.2026
 * @authors Florian Wiesner
 * @details Host unit tests for KnxTelegram (the event handed to subscribe/onTelegram callbacks)
 *          and KnxCodec::decodeTelegram (the shared inline-6 vs. payload decode).
 *          Run with: pio test -e native
*/

#include <unity.h>
#include "KnxFrame.h"
#include "KnxCodec.h"
#include "KnxTelegram.h"

static const PhysicalAddress SRC = { 1, 1, 7 };

// APCI command bits in octet 7 of a standard group frame.
static const uint8_t CMD_READ     = 0x00;
static const uint8_t CMD_RESPONSE = 0x40;

static uint8_t frame[KnxFrame::MAX_FRAME];

// Builds a GroupValueWrite for value on ga, optionally re-typed to Read/Response, and parses it.
static ParsedTelegram makeTelegram(GroupAddress ga, const KnxValue& value, int cmdBits = -1) {
	uint8_t n = KnxFrame::build(SRC, packGroupAddress(ga), value, frame, sizeof(frame));
	TEST_ASSERT_TRUE(n > 0);
	if (cmdBits == CMD_READ) {
		frame[7] = 0x00;                       // Read carries no data bits
	}
	else if (cmdBits >= 0) {
		frame[7] = (uint8_t)((frame[7] & 0x3F) | cmdBits);
	}
	frame[n - 1] = KnxFrame::checksum(frame, n - 1);
	ParsedTelegram p;
	TEST_ASSERT_TRUE(KnxFrame::parse(frame, n, p));
	return p;
}

void setUp(void) {}
void tearDown(void) {}

void test_decode_telegram_inline_dpt1(void) {
	ParsedTelegram p = makeTelegram(GroupAddress{ 0, 3, 0 }, Dpt1(true));
	TEST_ASSERT_TRUE(KnxCodec::decodeTelegram(KnxDpt::DPT1, p).asBool());
}

void test_decode_telegram_payload_dpt9(void) {
	ParsedTelegram p = makeTelegram(GroupAddress{ 0, 4, 2 }, Dpt9(21.5f));
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.5f, KnxCodec::decodeTelegram(KnxDpt::DPT9, p).asFloat());
}

void test_telegram_accessors(void) {
	ParsedTelegram p = makeTelegram(GroupAddress{ 0, 3, 0 }, Dpt1(true));
	KnxTelegram t(p);
	TEST_ASSERT_EQUAL_UINT16(packGroupAddress(GroupAddress{ 0, 3, 0 }), t.groupAddress());
	TEST_ASSERT_EQUAL_UINT8(7, t.source().device);
	TEST_ASSERT_TRUE(t.type() == GroupValueType::Write);
	TEST_ASSERT_FALSE(t.isRead());
	TEST_ASSERT_FALSE(t.value().isValid());
}

void test_telegram_decode_read_is_invalid(void) {
	ParsedTelegram p = makeTelegram(GroupAddress{ 0, 3, 0 }, Dpt1(true), CMD_READ);
	KnxTelegram t(p);
	TEST_ASSERT_TRUE(t.isRead());
	TEST_ASSERT_FALSE(t.decode(KnxDpt::DPT1).isValid());
}

void test_telegram_decode_response(void) {
	ParsedTelegram p = makeTelegram(GroupAddress{ 0, 3, 0 }, Dpt1(true), CMD_RESPONSE);
	KnxTelegram t(p);
	TEST_ASSERT_TRUE(t.type() == GroupValueType::Response);
	TEST_ASSERT_TRUE(t.decode(KnxDpt::DPT1).asBool());
}

void test_telegram_value_passthrough(void) {
	ParsedTelegram p = makeTelegram(GroupAddress{ 0, 4, 2 }, Dpt9(3.0f));
	KnxTelegram t(p, Dpt9(3.0f));
	TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, t.value().asFloat());
}

void test_telegram_raw_inline(void) {
	ParsedTelegram p = makeTelegram(GroupAddress{ 0, 3, 0 }, Dpt1(true));
	KnxTelegram t(p);
	TEST_ASSERT_EQUAL_UINT8(1, t.payloadLength());
	TEST_ASSERT_TRUE(t.payload()[0] & 0x01);
}

int main(int argc, char** argv) {
	(void)argc; (void)argv;
	UNITY_BEGIN();
	RUN_TEST(test_decode_telegram_inline_dpt1);
	RUN_TEST(test_decode_telegram_payload_dpt9);
	RUN_TEST(test_telegram_accessors);
	RUN_TEST(test_telegram_decode_read_is_invalid);
	RUN_TEST(test_telegram_decode_response);
	RUN_TEST(test_telegram_value_passthrough);
	RUN_TEST(test_telegram_raw_inline);
	return UNITY_END();
}
