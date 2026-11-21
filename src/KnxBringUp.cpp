// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name KnxBringUp.cpp
 * @date 21.11.2026
 * @authors Florian Wiesner
 * @details See KnxBringUp.h.
*/

//----Libraries----
#include "KnxBringUp.h"

using namespace KnxTransceiverProtocol;

void KnxBringUp::start(KnxTransceiver chip, PhysicalAddress address, uint32_t nowMs) {
	this->chip    = chip;
	this->address = address;
	this->nowMs   = nowMs;
	failed        = Step::Done;
	statusRaw     = 0;
	configureRaw  = 0;
	stateRaw      = 0xFF;
	frameGuard.reset();
	enter(Step::Reset);
}

// Sets up step s: its command, its deadline, and clears the per-step flags.
void KnxBringUp::enter(Step s) {
	step               = s;
	sent               = false;
	repollPending      = false;
	awaitingStatusByte = false;
	cmdLen             = 0;

	switch (s) {
		case Step::Reset:
			cmd[0]   = U_RESET_REQ;
			cmdLen   = 1;
			deadline = nowMs + resetTimeoutMs(chip);
			break;
		case Step::SystemStatus:
			cmd[0]   = NCN5130_SYSTEMSTAT;
			cmdLen   = 1;
			deadline = nowMs + SYSTEMSTAT_TIMEOUT_MS;
			break;
		case Step::SetAddress:
			cmdLen   = buildSetAddress(chip, address, cmd);
			deadline = nowMs;   // no answer expected; tick() moves on once it was sent
			break;
		case Step::AwaitConfigure:
			deadline = nowMs + addressConfirmTimeoutMs(chip);
			sent     = true;   // nothing to send: the timeout runs from now
			break;
		case Step::State:
			cmd[0]   = U_STATE_REQ;
			cmdLen   = 1;
			deadline = nowMs + STATE_TIMEOUT_MS;
			break;
		default:
			break;
	}
}

// Moves to the step after the current one, skipping steps this chip does not have.
void KnxBringUp::enterNext(void) {
	switch (step) {
		case Step::Reset:
			enter(hasSystemStatus(chip) ? Step::SystemStatus : Step::SetAddress);
			break;
		case Step::SystemStatus:
			enter(Step::SetAddress);
			break;
		case Step::SetAddress:
			enter(confirmsAddress(chip) ? Step::AwaitConfigure : Step::State);
			break;
		case Step::AwaitConfigure:
			enter(Step::State);
			break;
		case Step::State:
			step = Step::Done;
			break;
		default:
			break;
	}
}

// Ends the bring-up, remembering which step failed.
void KnxBringUp::fail(void) {
	failed = step;
	step   = Step::Failed;
	cmdLen = 0;
}

bool KnxBringUp::nextCommand(const uint8_t*& out, uint8_t& len) {
	if (finished() || cmdLen == 0) return false;
	out    = cmd;
	len    = cmdLen;
	cmdLen = 0;
	sent   = true;
	return true;
}

void KnxBringUp::feed(uint8_t byte, uint32_t nowMs) {
	this->nowMs = nowMs;
	if (finished()) return;

	// The byte after 0x4B is the status byte, whatever its value.
	if (awaitingStatusByte) {
		awaitingStatusByte = false;
		statusRaw = byte;
		if (decodeSystemStatus(byte).mode == MODE_NORMAL) {
			enterNext();
		} else {
			// Still in Power-Up or Sync: ask again shortly, until the step times out.
			repollPending = true;
			repollAt      = nowMs + SYSTEMSTAT_REPOLL_MS;
		}
		return;
	}

	// Bytes belonging to a bus frame are never answers. A byte that starts a frame leaves
	// the guard in-frame, the frame's last byte leaves it idle but was in-frame before.
	const bool wasInFrame = frameGuard.inFrame();
	frameGuard.feed(byte);
	if (wasInFrame || frameGuard.inFrame()) return;

	switch (step) {
		case Step::Reset:
			if (isResetIndication(byte)) enterNext();
			break;
		case Step::SystemStatus:
			if (byte == NCN5130_SYSTEMSTAT_IND) awaitingStatusByte = true;
			break;
		case Step::AwaitConfigure:
			if (isConfigureIndication(byte)) {
				configureRaw = byte;
				// Without auto-acknowledge the chip would not ACK telegrams to us.
				if (autoAcknowledgeActive(byte)) enterNext();
				else fail();
			}
			break;
		case Step::State:
			if (isStateIndication(byte)) {
				stateRaw = byte;
				if (byte == U_STATE_IND_OK) enterNext();
				else fail();
			}
			break;
		default:
			break;
	}
}

void KnxBringUp::tick(uint32_t nowMs) {
	this->nowMs = nowMs;
	if (finished()) return;

	if (step == Step::SetAddress) {
		if (sent) enterNext();
		return;
	}
	if (!sent) return;   // the timeout runs from the moment the command went out

	if (repollPending && expired(repollAt)) {
		repollPending = false;
		cmd[0] = NCN5130_SYSTEMSTAT;
		cmdLen = 1;
	}
	if (expired(deadline)) fail();
}
