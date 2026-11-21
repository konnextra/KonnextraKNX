/**
 * @name test_bringup.cpp
 * @date 21.11.2026
 * @authors Florian Wiesner
 * @details Host unit tests for KnxBringUp: the command sequence per chip, timeouts, and bus
 *          frames arriving mid-bring-up whose payload holds answer-like bytes (0x21, 0x07,
 *          0x4B). Simulated time, scripted chip answers. Run with: pio test -e native
*/

#include <unity.h>
#include <string.h>
#include "KnxBringUp.h"

typedef KnxBringUp::Step Step;

void setUp(void) {}
void tearDown(void) {}

static const PhysicalAddress ADDR = { 1, 1, 5 };

// A valid frame from 1.1.1 to 0/1/33 whose payload carries 0x21, 0x07 and 0x4B.
// Control 0xBC, src 11 01, dst 00 21, NPCI E3 (TPCI + 3 octets), APDU 00 80 07 4B, checksum.
static uint8_t TRAP_FRAME[11] = { 0xBC, 0x11, 0x01, 0x00, 0x21, 0xE3, 0x00, 0x80, 0x07, 0x4B, 0x00 };

static void fixChecksum(void) {
	uint8_t x = 0;
	for (uint8_t i = 0; i < 10; i++) x ^= TRAP_FRAME[i];
	TRAP_FRAME[10] = (uint8_t)~x;
}

// Records every command the state machine hands out.
struct Harness {
	KnxBringUp sm;
	uint32_t   now = 0;
	uint8_t    sentBytes[64];
	uint8_t    sentLen = 0;

	void pump(void) {
		const uint8_t* cmd;
		uint8_t len;
		sm.tick(now);
		while (sm.nextCommand(cmd, len)) {
			memcpy(sentBytes + sentLen, cmd, len);
			sentLen += len;
			sm.tick(now);
		}
	}
	void rx(const uint8_t* bytes, uint8_t n) {
		for (uint8_t i = 0; i < n; i++) sm.feed(bytes[i], now);
		pump();
	}
	void rx1(uint8_t b) { rx(&b, 1); }
	void wait(uint32_t ms) { now += ms; pump(); }
};

void test_ncn5130_quiet_bus(void) {
	Harness h;
	h.sm.start(KnxTransceiver::Ncn5130, ADDR, 0);
	h.pump();
	h.rx1(0x03);                         // Reset.ind
	const uint8_t sysstat[] = { 0x4B, 0xFB };
	h.rx(sysstat, 2);                    // SystemStat.ind, Normal
	h.wait(1);
	h.rx1(0x21);                         // Configure.ind, aa on
	h.rx1(0x07);                         // State.ind OK

	TEST_ASSERT_TRUE(h.sm.succeeded());
	const uint8_t expected[] = { 0x01, 0x0D, 0xF1, 0x11, 0x05, 0x00, 0x02 };
	TEST_ASSERT_EQUAL_UINT8(sizeof(expected), h.sentLen);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, h.sentBytes, sizeof(expected));
	TEST_ASSERT_EQUAL_HEX8(0x21, h.sm.configureByte());
	TEST_ASSERT_EQUAL_HEX8(0xFB, h.sm.systemStatusByte());
}

void test_tpuart2_sequence(void) {
	Harness h;
	h.sm.start(KnxTransceiver::TpUart2, ADDR, 0);
	h.pump();
	h.rx1(0x03);
	h.rx1(0x07);

	TEST_ASSERT_TRUE(h.sm.succeeded());
	const uint8_t expected[] = { 0x01, 0x28, 0x11, 0x05, 0x02 };
	TEST_ASSERT_EQUAL_UINT8(sizeof(expected), h.sentLen);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, h.sentBytes, sizeof(expected));
}

void test_frame_before_configure_is_not_an_answer(void) {
	fixChecksum();
	Harness h;
	h.sm.start(KnxTransceiver::Ncn5130, ADDR, 0);
	h.pump();
	h.rx1(0x03);
	const uint8_t sysstat[] = { 0x4B, 0xFB };
	h.rx(sysstat, 2);
	h.wait(1);
	h.rx(TRAP_FRAME, sizeof(TRAP_FRAME));
	TEST_ASSERT_FALSE(h.sm.finished());
	TEST_ASSERT_EQUAL_HEX8(0x00, h.sm.configureByte());   // 0x21 in the frame was skipped
	h.rx1(0x21);
	h.rx1(0x07);
	TEST_ASSERT_TRUE(h.sm.succeeded());
}

void test_frame_before_state_is_not_an_answer(void) {
	fixChecksum();
	Harness h;
	h.sm.start(KnxTransceiver::TpUart2, ADDR, 0);
	h.pump();
	h.rx1(0x03);
	h.rx(TRAP_FRAME, sizeof(TRAP_FRAME));
	TEST_ASSERT_FALSE(h.sm.finished());
	h.rx1(0x07);
	TEST_ASSERT_TRUE(h.sm.succeeded());
}

void test_reset_timeout(void) {
	Harness h;
	h.sm.start(KnxTransceiver::Ncn5130, ADDR, 0);
	h.pump();
	h.wait(199);
	TEST_ASSERT_FALSE(h.sm.finished());
	h.wait(1);
	TEST_ASSERT_TRUE(h.sm.finished());
	TEST_ASSERT_FALSE(h.sm.succeeded());
	TEST_ASSERT_EQUAL(Step::Reset, h.sm.failedAt());
}

void test_configure_timeout(void) {
	Harness h;
	h.sm.start(KnxTransceiver::Ncn5130, ADDR, 0);
	h.pump();
	h.rx1(0x03);
	const uint8_t sysstat[] = { 0x4B, 0xFB };
	h.rx(sysstat, 2);
	h.wait(150);
	TEST_ASSERT_EQUAL(Step::AwaitConfigure, h.sm.failedAt());
}

void test_sync_mode_repolls_then_succeeds(void) {
	Harness h;
	h.sm.start(KnxTransceiver::Ncn5130, ADDR, 0);
	h.pump();
	h.rx1(0x03);
	const uint8_t sync[] = { 0x4B, 0xF9 };    // mode 01 = Sync
	h.rx(sync, 2);
	h.wait(10);
	TEST_ASSERT_EQUAL_HEX8(0x0D, h.sentBytes[h.sentLen - 1]);   // asked again
	const uint8_t normal[] = { 0x4B, 0xFB };
	h.rx(normal, 2);
	h.wait(1);
	h.rx1(0x21);
	h.rx1(0x07);
	TEST_ASSERT_TRUE(h.sm.succeeded());
}

void test_stuck_in_sync_fails_at_system_status(void) {
	Harness h;
	h.sm.start(KnxTransceiver::Ncn5130, ADDR, 0);
	h.pump();
	h.rx1(0x03);
	const uint8_t sync[] = { 0x4B, 0xF9 };
	for (uint8_t i = 0; i < 25; i++) { h.rx(sync, 2); h.wait(10); }
	TEST_ASSERT_TRUE(h.sm.finished());
	TEST_ASSERT_EQUAL(Step::SystemStatus, h.sm.failedAt());
}

void test_idle_zero_bytes_ignored(void) {
	Harness h;
	h.sm.start(KnxTransceiver::TpUart2, ADDR, 0);
	h.pump();
	h.rx1(0x00);
	h.rx1(0x03);
	h.rx1(0x00);
	h.rx1(0x07);
	TEST_ASSERT_TRUE(h.sm.succeeded());
}

void test_configure_without_auto_ack_fails(void) {
	Harness h;
	h.sm.start(KnxTransceiver::Ncn5130, ADDR, 0);
	h.pump();
	h.rx1(0x03);
	const uint8_t sysstat[] = { 0x4B, 0xFB };
	h.rx(sysstat, 2);
	h.wait(1);
	h.rx1(0x01);
	TEST_ASSERT_EQUAL(Step::AwaitConfigure, h.sm.failedAt());
}

void test_state_with_error_flags_fails(void) {
	Harness h;
	h.sm.start(KnxTransceiver::TpUart2, ADDR, 0);
	h.pump();
	h.rx1(0x03);
	h.rx1(0x0F);   // TW flag set
	TEST_ASSERT_EQUAL(Step::State, h.sm.failedAt());
	TEST_ASSERT_EQUAL_HEX8(0x0F, h.sm.stateByte());
}

int main(int, char**) {
	UNITY_BEGIN();
	RUN_TEST(test_ncn5130_quiet_bus);
	RUN_TEST(test_tpuart2_sequence);
	RUN_TEST(test_frame_before_configure_is_not_an_answer);
	RUN_TEST(test_frame_before_state_is_not_an_answer);
	RUN_TEST(test_reset_timeout);
	RUN_TEST(test_configure_timeout);
	RUN_TEST(test_sync_mode_repolls_then_succeeds);
	RUN_TEST(test_stuck_in_sync_fails_at_system_status);
	RUN_TEST(test_idle_zero_bytes_ignored);
	RUN_TEST(test_configure_without_auto_ack_fails);
	RUN_TEST(test_state_with_error_flags_fails);
	return UNITY_END();
}
