#pragma once
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name KnxDriver.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Concrete link-layer driver for a UART bus coupler: the onsemi NCN5130 (default)
 *          or the Siemens TP-UART2 and compatibles, incl. the STKNX-behind-ATTiny front end.
 *          Implements IKnxDriver: port bring-up (chip-specific, via KnxBringUp), per-byte telegram transmission with a real L_Data.con result
 *          (PLAN §9), and byte-stream RX via KnxReassembler.
 *          Replaces the thesis KNX_TPUART2 and is decoupled from the telegram/coordinator
 *          layers — the coordinator injects it as an IKnxDriver*.
 *
 *          The port is injected, never constructed: the driver holds a reference to one the
 *          core already provides (Serial1 and friends). That is what makes it buildable on
 *          every Arduino core rather than only on ESP32, and it is why there is no
 *          architecture guard anywhere below.
 *
 *          Timing and con-byte values follow the TP-UART2 spec and are verified on hardware —
 *          against the STKNX/ATTiny front end and against a standard TP-UART2 transceiver. The
 *          NCN5130 shares the send path, echo and con byte; only bring-up differs.
*/

//---- Standard / platform libraries ----
#include <Arduino.h>

//---- Custom shared types ----
#include "KnxInterfaces.h"
#include "KnxAddress.h"
#include "KnxEnums.h"
#include "KnxDebug.h"

//---- Other custom module headers ----
#include "KnxReassembler.h"
#include "KnxTransceiverProtocol.h"
#include "KnxBringUp.h"

// The port used by the address-only constructor. Resolved once, here, so the rest of the
// driver never names a board or an architecture. Override it per project if the front end
// hangs off a different UART:  build_flags = -DKNX_DEFAULT_PORT=Serial2
#ifndef KNX_DEFAULT_PORT
	#if defined(SERIAL_PORT_HARDWARE_OPEN)
		// Arduino's own answer to "the first hardware UART whose pins are not already
		// dedicated to something else". Correctly absent on boards with only one UART.
		#define KNX_DEFAULT_PORT SERIAL_PORT_HARDWARE_OPEN
	#elif defined(HAVE_HWSERIAL1)
		// AVR and STM32duino both use this name, and both set it only where Serial1 is
		// really instantiated. Asking the core beats guessing from the architecture:
		// STM32duino *declares* Serial1 whenever the chip has a USART1 but only defines
		// it when the sketch enables it, so an architecture test would compile and then
		// fail at link with "undefined reference to Serial1".
		#define KNX_DEFAULT_PORT Serial1
	#elif defined(ARDUINO_ARCH_ESP32)   || defined(ARDUINO_ARCH_RP2040) || \
	      defined(ARDUINO_ARCH_RENESAS) || defined(ARDUINO_ARCH_SAMD)   || \
	      defined(ARDUINO_ARCH_MBED)
		// Cores that always provide Serial1 and expose no macro to ask.
		#define KNX_DEFAULT_PORT Serial1
	#endif
#endif

class KnxDriver : public IKnxDriver {
	private:
		//---- Timing ----
		static constexpr uint16_t CON_TIMEOUT_MS   = 100;  // L_Data.con window after TX
		static constexpr uint16_t READY_TIMEOUT_MS = 1000; // wait for the reset pin to go high
		static constexpr uint8_t  RESET_PULSE_MS   = 2;    // TP-UART2 hard-reset pulse on RESn
		static constexpr uint8_t  TPUART2_SETTLE_MS = 50;  // datasheet: wait 50 ms after RESn high

		//---- Link settings ----
		// The line format both chips use (the NCN5130 board is strapped for 9-bit UART, which
		// is 8E1). SERIAL_8E1 is never cached in a member: AVR types the config parameter as uint8_t, the Arduino core API as uint16_t, so it is passed
		// straight to begin() at the call site instead.
		static constexpr uint32_t BAUDRATE = 19200;

		//---- Members ----
		// p_io carries every read and write and is always set. p_uart is set only when the
		// driver was handed a HardwareSerial and therefore owns the line configuration; on
		// the Stream path it stays null and begin() leaves the port alone.
		Stream*         p_io   = nullptr;
		HardwareSerial* p_uart = nullptr;

		KnxReassembler  reassembler;
		PhysicalAddress physicalAddress;
		KnxTransceiver  chip;        // fixed at construction
		KnxBringUp      bringUpSm;

		// Optional status pins, -1 = not connected. Both are open-drain lines with a pull-up
		// inside the chip; the driver never drives them high.
		int             resetPin      = -1;
		int             savePin       = -1;
		bool            lastResetHigh = true;
		bool            lastSaveHigh  = true;
		void (*p_onPowerFail)(void)   = nullptr;

		//---- Private methods ----
		// Writes a raw command byte sequence to the transceiver.
		void sendCommand(const uint8_t* cmd, uint16_t len);
		// Shared by begin() and reset(): runs KnxBringUp to completion, pumping its commands to
		// the port and RX bytes into it, then logs the outcome. Returns bringUpSm.succeeded().
		bool bringUp(const char* phase);
		// Consumes the transmit echo, then reads the L_Data.con within CON_TIMEOUT_MS.
		bool awaitConfirmation(const uint8_t* frame, uint8_t length);
		// Before a bring-up: hard-resets a TP-UART2 via RESn, then waits for the reset pin to
		// go high. No-op without a reset pin. Returns false if the chip stays in reset.
		bool prepareChip(void);
		// Watches the status pins: power-fail callback on SAVE falling, re-bring-up on the
		// reset pin rising. Returns false while the chip is held in reset.
		bool checkStatusPins(void);
		// True if a reset pin is connected and currently low.
		bool chipInReset(void) const;

	public:
		//---- Constructors ----
#ifdef KNX_DEFAULT_PORT
		/**
		 * @brief Constructs the driver on this board's default KNX port.
		 * @details The port is `Serial1` on most boards; begin() opens it at 19200 8E1.
		 *          Override the choice with `-DKNX_DEFAULT_PORT=Serial2`.
		 * @param physicalAddress Physical address of this device (e.g. "1.1.5").
		 * @param chip            The bus coupler chip: KnxTransceiver::Ncn5130 (default) or
		 *                        KnxTransceiver::TpUart2.
		*/
		KnxDriver(String physicalAddress, KnxTransceiver chip = KnxTransceiver::Ncn5130);
#else
		// This board has no hardware UART free for KNX — its only port is the console.
		// Pass one explicitly instead:  KonnextraKNX knx("1.1.5", Serial);
		KnxDriver(String physicalAddress, KnxTransceiver chip = KnxTransceiver::Ncn5130) = delete;
#endif

		/**
		 * @brief Constructs the driver on a port you name; begin() opens it at 19200 8E1.
		 * @param physicalAddress Physical address of this device (e.g. "1.1.5").
		 * @param port            The serial port the transceiver is wired to.
		 * @param chip            The bus coupler chip: KnxTransceiver::Ncn5130 (default) or
		 *                        KnxTransceiver::TpUart2.
		*/
		KnxDriver(String physicalAddress, HardwareSerial& port,
		          KnxTransceiver chip = KnxTransceiver::Ncn5130);

		/**
		 * @brief Constructs the driver on a stream you have already configured yourself.
		 * @details begin() does not touch the line settings on this path, so the stream must
		 *          already be open at 19200 8E1 — KNX will not work at any other setting.
		 * @param physicalAddress Physical address of this device (e.g. "1.1.5").
		 * @param stream          An open stream connected to the transceiver.
		 * @param chip            The bus coupler chip: KnxTransceiver::Ncn5130 (default) or
		 *                        KnxTransceiver::TpUart2.
		*/
		KnxDriver(String physicalAddress, Stream& stream,
		          KnxTransceiver chip = KnxTransceiver::Ncn5130);

		/**
		 * @brief The bus coupler chip this driver was configured for.
		 * @return KnxTransceiver::Ncn5130 or KnxTransceiver::TpUart2.
		*/
		KnxTransceiver transceiver(void) const { return chip; }

		/**
		 * @brief Connects the chip's optional reset and save status pins. Call before begin().
		 * @details Both are open-drain lines with a pull-up inside the chip; the driver
		 *          only reads them, except that on a TP-UART2 it pulls RESn low for a hard
		 *          reset. See KonnextraKNX::setStatusPins() for the user-facing description.
		 * @param resetPin MCU pin wired to RESETB (NCN5130) or RESn (TP-UART2); -1 if unused.
		 * @param savePin  MCU pin wired to SAVEB (NCN5130) or SAVE (TP-UART2); -1 if unused.
		*/
		void setStatusPins(int resetPin, int savePin = -1);

		/**
		 * @brief Registers a function called once when the save pin reports a bus power failure.
		 * @param callback Called from loop(); keep it short. nullptr removes it.
		*/
		void onBusPowerFail(void (*callback)(void)) { p_onPowerFail = callback; }

		/**
		 * @brief Whether the bus power is good according to the save pin.
		 * @return false while the save pin is low; always true without a save pin.
		*/
		bool busPowerOk(void) const;

		//---- IKnxDriver ----
		/**
		 * @brief Opens the port (8E1), resets the transceiver and applies the address.
		 * @return true if the transceiver reports State OK after bring-up.
		*/
		bool begin(void) override;

		/**
		 * @brief Resets the transceiver and re-applies the physical address.
		 * @return true if the transceiver reports State OK.
		*/
		bool reset(void) override;

		/**
		 * @brief Transmits an assembled telegram byte-by-byte and reads the L_Data.con.
		 * @param frame  Pointer to the assembled frame bytes (incl. checksum).
		 * @param length Number of bytes in frame.
		 * @return true if the transceiver returned a positive L_Data.con (PLAN §9).
		*/
		bool sendTelegram(const uint8_t* frame, uint8_t length) override;

		/**
		 * @brief Consumes available RX bytes; on a complete reassembled frame copies it out.
		 * @param out    Destination buffer for a complete frame.
		 * @param maxLen Capacity of out.
		 * @param outLen Set to the frame length when true is returned.
		 * @return true if a complete frame is available this call (at most one per call).
		*/
		bool poll(uint8_t* out, uint8_t maxLen, uint8_t& outLen) override;
};
