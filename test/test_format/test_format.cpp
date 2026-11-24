/**
 * @name test_format.cpp
 * @date 24.11.2026
 * @authors Florian Wiesner
 * @details Known-answer tests for KnxFormat, the text rendering behind the Basic/Deep log lines.
 *          Run with: pio test -e native
*/

#include <unity.h>
#include <string.h>
#include "KnxFrame.h"
#include "KnxFormat.h"

static char buf[96];
static uint8_t frame[KnxFrame::MAX_FRAME];
static uint8_t frameLen = 0;

static const char* fmtValue(const KnxValue& v) { KnxFormat::value(v, buf, sizeof(buf)); return buf; }

static ParsedTelegram group(GroupAddress ga, const KnxValue& v, int cmdBits = -1) {
	frameLen = KnxFrame::build(PhysicalAddress{ 1, 1, 1 }, packGroupAddress(ga), v, frame, sizeof(frame));
	if (cmdBits == 0x00)    frame[7] = 0x00;
	else if (cmdBits >= 0)  frame[7] = (uint8_t)((frame[7] & 0x3F) | cmdBits);
	frame[frameLen - 1] = KnxFrame::checksum(frame, frameLen - 1);
	ParsedTelegram p;
	TEST_ASSERT_TRUE(KnxFrame::parse(frame, frameLen, p));
	return p;
}

void setUp(void) {}
void tearDown(void) {}

void test_value_scalars(void) {
	TEST_ASSERT_EQUAL_STRING("DPT1: on", fmtValue(Dpt1(true)));
	TEST_ASSERT_EQUAL_STRING("DPT1: off", fmtValue(Dpt1(false)));
	TEST_ASSERT_EQUAL_STRING("DPT2: 3", fmtValue(Dpt2(3)));
	TEST_ASSERT_EQUAL_STRING("DPT5: 128", fmtValue(Dpt5(128)));
	TEST_ASSERT_EQUAL_STRING("DPT6: -12", fmtValue(Dpt6(-12)));
	TEST_ASSERT_EQUAL_STRING("DPT7: 65535", fmtValue(Dpt7(65535)));
	TEST_ASSERT_EQUAL_STRING("DPT8: -32768", fmtValue(Dpt8(-32768)));
	TEST_ASSERT_EQUAL_STRING("DPT12: 4000000000", fmtValue(Dpt12(4000000000UL)));
	TEST_ASSERT_EQUAL_STRING("DPT13: -7", fmtValue(Dpt13(-7)));
	TEST_ASSERT_EQUAL_STRING("DPT4: 'A'", fmtValue(Dpt4('A')));
}

void test_value_floats(void) {
	TEST_ASSERT_EQUAL_STRING("DPT9: 21.50", fmtValue(Dpt9(21.5f)));
	TEST_ASSERT_EQUAL_STRING("DPT9: -5.25", fmtValue(Dpt9(-5.25f)));
	TEST_ASSERT_EQUAL_STRING("DPT9: 0.00", fmtValue(Dpt9(0.0f)));
	TEST_ASSERT_EQUAL_STRING("DPT9: 1.00", fmtValue(Dpt9(0.999f)));   // rounds up into the integer
	TEST_ASSERT_EQUAL_STRING("DPT14: 1.23e12", fmtValue(Dpt14(1.23e12f)));
	TEST_ASSERT_EQUAL_STRING("DPT14: -inf", fmtValue(Dpt14(-1.0f / 0.0f)));
}

void test_value_structured(void) {
	TEST_ASSERT_EQUAL_STRING("DPT3: +3", fmtValue(Dpt3(true, 3)));
	TEST_ASSERT_EQUAL_STRING("DPT3: -1", fmtValue(Dpt3(false, 1)));
	TEST_ASSERT_EQUAL_STRING("DPT3: stop", fmtValue(Dpt3(true, 0)));
	DptTime t = { 1, 13, 5, 9 };
	TEST_ASSERT_EQUAL_STRING("DPT10: Mon 13:05:09", fmtValue(Dpt10(t)));
	DptTime t0 = { 0, 7, 0, 0 };
	TEST_ASSERT_EQUAL_STRING("DPT10: 07:00:00", fmtValue(Dpt10(t0)));
	DptDate d = { 24, 11, 2026 };
	TEST_ASSERT_EQUAL_STRING("DPT11: 2026-11-24", fmtValue(Dpt11(d)));
	TEST_ASSERT_EQUAL_STRING("DPT232: #FF8000", fmtValue(Dpt232(255, 128, 0)));
}

void test_value_invalid(void) {
	KnxValue none;
	TEST_ASSERT_EQUAL_STRING("", fmtValue(none));
}

void test_telegram_write_inline(void) {
	ParsedTelegram p = group(GroupAddress{ 0, 1, 1 }, Dpt1(true));
	KnxFormat::telegram(p, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("1.1.1 -> 0/1/1  Write     01", buf);
}

void test_telegram_response_payload(void) {
	ParsedTelegram p = group(GroupAddress{ 0, 4, 2 }, Dpt9(21.5f), 0x40);
	KnxFormat::telegram(p, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("1.1.1 -> 0/4/2  Response  0C 33", buf);
}

void test_telegram_read_has_no_data(void) {
	ParsedTelegram p = group(GroupAddress{ 0, 3, 0 }, Dpt1(true), 0x00);
	KnxFormat::telegram(p, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("1.1.1 -> 0/3/0  Read", buf);
}

void test_telegram_individual(void) {
	frameLen = KnxFrame::buildControl(PhysicalAddress{ 1, 1, 9 }, PhysicalAddress{ 1, 1, 5 },
	                                  KnxTpci::Connect, 0, frame, sizeof(frame));
	ParsedTelegram p;
	TEST_ASSERT_TRUE(KnxFrame::parse(frame, frameLen, p));
	KnxFormat::telegram(p, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("1.1.9 -> 1.1.5  Connect", buf);
}

void test_frame_details(void) {
	group(GroupAddress{ 0, 1, 1 }, Dpt1(true));   // BC 11 01 01 01 E1 00 81 cs
	KnxFormat::frameControl(frame, frameLen, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("ctrl BC  priority low  repeat no", buf);
	KnxFormat::frameRouting(frame, frameLen, buf, sizeof(buf));
	char want[48];
	strcpy(want, "hops 6  length 1  checksum ");
	static const char hex[] = "0123456789ABCDEF";
	uint8_t cs = frame[frameLen - 1];
	size_t n = strlen(want);
	want[n] = hex[cs >> 4]; want[n + 1] = hex[cs & 0x0F]; want[n + 2] = '\0';
	strcat(want, " ok");
	TEST_ASSERT_EQUAL_STRING(want, buf);
}

void test_truncation_is_safe(void) {
	char small[8];
	memset(small, 'X', sizeof(small));
	uint8_t n = KnxFormat::value(Dpt9(21.5f), small, 6);
	TEST_ASSERT_EQUAL_UINT8(5, n);
	TEST_ASSERT_EQUAL_STRING("DPT9:", small);
	TEST_ASSERT_EQUAL_CHAR('X', small[6]);   // nothing written past max
}

int main(int argc, char** argv) {
	(void)argc; (void)argv;
	UNITY_BEGIN();
	RUN_TEST(test_value_scalars);
	RUN_TEST(test_value_floats);
	RUN_TEST(test_value_structured);
	RUN_TEST(test_value_invalid);
	RUN_TEST(test_telegram_write_inline);
	RUN_TEST(test_telegram_response_payload);
	RUN_TEST(test_telegram_read_has_no_data);
	RUN_TEST(test_telegram_individual);
	RUN_TEST(test_frame_details);
	RUN_TEST(test_truncation_is_safe);
	return UNITY_END();
}
