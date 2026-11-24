/**
 * @name test_debug.cpp
 * @date 24.11.2026
 * @authors Florian Wiesner
 * @details Host tests for the log levels: which line kinds each level prints and their exact
 *          shape, captured through KnxDebug's line sink. Run with: pio test -e native
*/

#include <unity.h>
#include <string.h>
#include "KnxDebug.h"

static char lines[8][128];
static int  lineCount = 0;

static void capture(const char* line) {
	if (lineCount < 8) { strncpy(lines[lineCount], line, 127); lines[lineCount][127] = '\0'; }
	lineCount++;
}

void setUp(void) { lineCount = 0; KnxDebug::setLineSink(capture); }
void tearDown(void) { KnxDebug::setLineSink(nullptr); KnxDebug::setLevel(KnxLog::Off); }

static void emitAllKinds(void) {
	KnxDebug::info("info %d", 1);
	KnxDebug::problem("problem");
	KnxDebug::section("TX");
	KnxDebug::banner("START");
	KnxDebug::log("detail");
	KnxDebug::event(true, "power");
}

void test_off_prints_nothing(void) {
	KnxDebug::setLevel(KnxLog::Off);
	emitAllKinds();
	TEST_ASSERT_EQUAL_INT(0, lineCount);
}

void test_basic_kinds(void) {
	KnxDebug::setLevel(KnxLog::Basic);
	emitAllKinds();
	TEST_ASSERT_EQUAL_INT(3, lineCount);
	TEST_ASSERT_EQUAL_STRING("[knx] info 1", lines[0]);
	TEST_ASSERT_EQUAL_STRING("[knx] ! problem  (more: setDebugMode(KnxLog::Deep))", lines[1]);
	TEST_ASSERT_EQUAL_STRING("[knx] ! power  (more: setDebugMode(KnxLog::Deep))", lines[2]);
}

void test_deep_kinds(void) {
	KnxDebug::setLevel(KnxLog::Deep);
	emitAllKinds();
	TEST_ASSERT_EQUAL_INT(6, lineCount);
	TEST_ASSERT_EQUAL_STRING("[knx] info 1", lines[0]);
	TEST_ASSERT_EQUAL_STRING("[knx] ! problem", lines[1]);                // no hint at Deep
	TEST_ASSERT_EQUAL_INT(6 + 64, (int)strlen(lines[2]));                // padded separator
	TEST_ASSERT_EQUAL_INT(0, strncmp(lines[2], "[knx] ----- TX ---", 18));
	TEST_ASSERT_EQUAL_INT(0, strncmp(lines[3], "[knx] ===== START ===", 21));
	TEST_ASSERT_EQUAL_STRING("[knx]   detail", lines[4]);
	TEST_ASSERT_EQUAL_INT(0, strncmp(lines[5], "[knx] ----- ! power ---", 23));
}

void test_bytes_deep_only(void) {
	const uint8_t b[] = { 0xBC, 0x11, 0x05 };
	KnxDebug::setLevel(KnxLog::Basic);
	KnxDebug::logBytes("tx frame:", b, 3);
	TEST_ASSERT_EQUAL_INT(0, lineCount);
	KnxDebug::setLevel(KnxLog::Deep);
	KnxDebug::logBytes("tx frame:", b, 3);
	TEST_ASSERT_EQUAL_STRING("[knx]   tx frame: BC 11 05", lines[0]);
}

void test_legacy_switch(void) {
	KnxDebug::setEnabled(true);
	TEST_ASSERT_TRUE(KnxDebug::isDeep());
	KnxDebug::setEnabled(false);
	TEST_ASSERT_FALSE(KnxDebug::isEnabled());
}

void test_hint_survives_long_line(void) {
	KnxDebug::setLevel(KnxLog::Basic);
	KnxDebug::problem("%s", "TX  15.15.255 -> 31/7/255  Write     12 34 56 78  (DPT14: 1234567.00)  not confirmed by the bus");
	const char* hint = "  (more: setDebugMode(KnxLog::Deep))";
	size_t n = strlen(lines[0]), h = strlen(hint);
	TEST_ASSERT_TRUE(n > h);
	TEST_ASSERT_EQUAL_STRING(hint, lines[0] + n - h);
}

int main(int argc, char** argv) {
	(void)argc; (void)argv;
	UNITY_BEGIN();
	RUN_TEST(test_off_prints_nothing);
	RUN_TEST(test_basic_kinds);
	RUN_TEST(test_deep_kinds);
	RUN_TEST(test_bytes_deep_only);
	RUN_TEST(test_legacy_switch);
	RUN_TEST(test_hint_survives_long_line);
	return UNITY_END();
}
