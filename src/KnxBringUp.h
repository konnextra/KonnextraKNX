#pragma once
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name KnxBringUp.h
 * @date 21.11.2026
 * @authors Florian Wiesner
 * @details Bring-up of a bus coupler chip as a state machine: reset, (system status), set the
 *          physical address, (await its confirmation), read the state. The steps in brackets
 *          are NCN5130-only. Arduino-free and host-tested; KnxDriver only pumps bytes and time
 *          through it.
 *
 *          The chip forwards bus frames at any time, also during bring-up, and its answers
 *          share byte values with ordinary frame payload (0x21 is a Configure.ind and a common
 *          address byte). A byte therefore counts as an answer only while no frame is being
 *          received and if it cannot start one — which an internal KnxReassembler tracks.
 *          Frames that complete during bring-up are dropped; no receiver is active yet.
*/

//---- Libraries ----
#include <stdint.h>

//---- Custom shared types ----
#include "KnxEnums.h"
#include "KnxAddress.h"

//---- Other custom module headers ----
#include "KnxTransceiverProtocol.h"
#include "KnxReassembler.h"

class KnxBringUp {
	public:
		// Bring-up steps in order. Done and Failed are terminal.
		enum class Step : uint8_t { Reset, SystemStatus, SetAddress, AwaitConfigure, State, Done, Failed };

	private:
		//---- Config ----
		static constexpr uint16_t SYSTEMSTAT_TIMEOUT_MS = 200;   // includes re-polls while in Sync
		static constexpr uint16_t SYSTEMSTAT_REPOLL_MS  = 10;
		static constexpr uint16_t STATE_TIMEOUT_MS      = 50;

		//---- Members ----
		KnxReassembler  frameGuard;   // only used to know whether a frame is in flight
		KnxTransceiver  chip    = KnxTransceiver::Ncn5130;
		PhysicalAddress address = { 0, 0, 0 };
		Step            step    = Step::Done;
		Step            failed  = Step::Done;
		uint32_t        nowMs    = 0;
		uint32_t        deadline = 0;
		uint32_t        repollAt = 0;
		bool            repollPending = false;
		bool            sent          = false;   // current step's command was handed out
		bool            awaitingStatusByte = false;
		uint8_t         cmd[KnxTransceiverProtocol::SET_ADDRESS_MAX_LEN];
		uint8_t         cmdLen       = 0;
		uint8_t         statusRaw    = 0;
		uint8_t         configureRaw = 0;
		uint8_t         stateRaw     = 0xFF;

		//---- Private methods ----
		void enter(Step s);
		void enterNext(void);
		void fail(void);
		bool expired(uint32_t at) const { return (int32_t)(nowMs - at) >= 0; }

	public:
		//---- Constructor ----
		/**
		 * @brief Constructs an idle bring-up; call start() to run one.
		*/
		KnxBringUp(void) = default;

		/**
		 * @brief Starts a bring-up for this chip and address.
		 * @param chip    The bus coupler chip on the port.
		 * @param address The physical address to apply.
		 * @param nowMs   Current time in milliseconds.
		*/
		void start(KnxTransceiver chip, PhysicalAddress address, uint32_t nowMs);

		/**
		 * @brief The command the driver must write next, if any; handed out once.
		 * @param out Set to the command bytes; valid until the next tick().
		 * @param len Set to the command length.
		 * @return true if a command is pending.
		*/
		bool nextCommand(const uint8_t*& out, uint8_t& len);

		/**
		 * @brief Feeds one byte received from the chip.
		 * @param byte  The received byte.
		 * @param nowMs Current time in milliseconds.
		*/
		void feed(uint8_t byte, uint32_t nowMs);

		/**
		 * @brief Advances steps that need no answer and enforces timeouts.
		 * @param nowMs Current time in milliseconds.
		*/
		void tick(uint32_t nowMs);

		/**
		 * @brief Whether the bring-up has ended, successfully or not.
		 * @return true once Done or Failed.
		*/
		bool finished(void) const { return step == Step::Done || step == Step::Failed; }

		/**
		 * @brief Whether the bring-up ended successfully.
		 * @return true once Done.
		*/
		bool succeeded(void) const { return step == Step::Done; }

		/**
		 * @brief The step that timed out or was answered wrongly.
		 * @return The failed step; Step::Done if nothing failed.
		*/
		Step failedAt(void) const { return failed; }

		/**
		 * @brief The raw U_SystemStat.ind status byte (NCN5130 only, 0 if none arrived).
		 * @return The status byte; decode it with KnxTransceiverProtocol::decodeSystemStatus().
		*/
		uint8_t systemStatusByte(void) const { return statusRaw; }

		/**
		 * @brief The raw U_Configure.ind byte (NCN5130 only, 0 if none arrived).
		 * @return The configure indication byte.
		*/
		uint8_t configureByte(void) const { return configureRaw; }

		/**
		 * @brief The raw U_State.ind byte (0xFF if none arrived).
		 * @return The state indication byte.
		*/
		uint8_t stateByte(void) const { return stateRaw; }
};
