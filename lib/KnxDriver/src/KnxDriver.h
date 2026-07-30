#pragma once
/**
 * @name KnxDriver.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Concrete link-layer driver for the STKNX-behind-ATTiny front end, which mirrors
 *          the Siemens TP-UART2 UART protocol. Implements IKnxDriver: port bring-up, reset/
 *          state handshake, per-byte telegram transmission with a real L_Data.con result
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
 *          against the STKNX/ATTiny front end and against a standard TP-UART2 transceiver.
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
		//---- TP-UART2 command bytes ----
		static constexpr uint8_t U_RESET_REQ           = 0x01;
		static constexpr uint8_t U_STATE_REQ           = 0x02;
		static constexpr uint8_t U_ACK_INFO_ACK        = 0x11;
		static constexpr uint8_t U_ACK_INFO_BUSY       = 0x12;
		static constexpr uint8_t U_ACK_INFO_NACK       = 0x14;
		static constexpr uint8_t U_SET_ADDRESS         = 0x28;
		static constexpr uint8_t U_DATA_START_CONTINUE = 0x80;
		static constexpr uint8_t U_DATA_END            = 0x40;

		//---- TP-UART2 response codes ----
		static constexpr uint8_t U_RESET_IND    = 0x03;
		static constexpr uint8_t U_STATE_IND_OK = 0x07;

		// L_Data.con: fixed low-nibble pattern 0x_B; bit 7 = positive confirmation.
		// Verified against reference/TP-UART2.pdf, which encodes L_DATA.confirm as 'x0001011'
		// with x = 1 positive — and against a bench run reporting 0x8B.
		static constexpr uint8_t CON_MASK      = 0x7F;
		static constexpr uint8_t CON_PATTERN   = 0x0B;
		static constexpr uint8_t CON_POSITIVE  = 0x80;

		//---- Timing ----
		static constexpr uint8_t  RESPONSE_TIME_MS = 10;   // U_ command response window
		static constexpr uint16_t CON_TIMEOUT_MS   = 100;  // L_Data.con window after TX

		//---- Link settings ----
		// The TP-UART2 line format. SERIAL_8E1 is never cached in a member: AVR types the
		// config parameter as uint8_t, the Arduino core API as uint16_t, so it is passed
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

		//---- Private methods ----
		// Writes a raw command byte sequence to the transceiver.
		void sendCommand(const uint8_t* cmd, uint16_t len);
		// Sends Reset.request and waits for Reset.indication.
		bool resetRequest(void);
		// Sends State.request and returns the raw State.indication byte (0xFF on timeout).
		uint8_t stateRequest(void);
		// Sends the stored physical address to the transceiver (address filtering / ACK).
		void applyPhysicalAddress(void);
		// Flushes pending RX bytes.
		void clearBuffer(void);
		// Consumes the transmit echo, then reads the L_Data.con within CON_TIMEOUT_MS.
		bool awaitConfirmation(const uint8_t* frame, uint8_t length);
		// Classifies a byte as an L_Data.con and its polarity.
		static bool isConfirmation(uint8_t b);
		static bool isPositiveConfirmation(uint8_t b);

	public:
		//---- Constructors ----
#ifdef KNX_DEFAULT_PORT
		/**
		 * @brief Constructs the driver on this board's default KNX port.
		 * @details The port is `Serial1` on most boards; begin() opens it at 19200 8E1.
		 *          Override the choice with `-DKNX_DEFAULT_PORT=Serial2`.
		 * @param physicalAddress Physical address of this device (e.g. "1.1.5").
		*/
		KnxDriver(String physicalAddress);
#else
		// This board has no hardware UART free for KNX — its only port is the console.
		// Pass one explicitly instead:  Konnextra knx("1.1.5", Serial);
		KnxDriver(String physicalAddress) = delete;
#endif

		/**
		 * @brief Constructs the driver on a port you name; begin() opens it at 19200 8E1.
		 * @param physicalAddress Physical address of this device (e.g. "1.1.5").
		 * @param port            The serial port the transceiver is wired to.
		*/
		KnxDriver(String physicalAddress, HardwareSerial& port);

		/**
		 * @brief Constructs the driver on a stream you have already configured yourself.
		 * @details begin() does not touch the line settings on this path, so the stream must
		 *          already be open at 19200 8E1 — KNX will not work at any other setting.
		 * @param physicalAddress Physical address of this device (e.g. "1.1.5").
		 * @param stream          An open stream connected to the transceiver.
		*/
		KnxDriver(String physicalAddress, Stream& stream);

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
