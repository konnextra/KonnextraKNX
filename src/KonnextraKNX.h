#pragma once
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name KonnextraKNX.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details The only header a sketch includes. It brings in the whole KNX library — the bus connection,
 *          the value types (KnxValue and the Dpt*() factories), and every device object (KnxLight,
 *          KnxBlind, KnxTemperature, …) — so a sketch needs just:
 *
 *              #include <KonnextraKNX.h>
 *              KonnextraKNX knx("1.1.5");                   // this device's KNX address (NCN5130)
 *              // KonnextraKNX knx("1.1.5", KnxTransceiver::TpUart2);   // on a TP-UART2
 *              KnxLight kitchen(knx, "0/1/1", "0/3/0");
*/

//---- Bus driver ----
#include "KnxDriver.h"

//---- Bus connection + value types ----
#include "KnxCoordinator.h"
#include "KnxValue.h"         // KnxValue + Dpt1(..)..Dpt232(..) factories

//---- Device objects, by domain ----
#include "KnxObject.h"        // generic object tier
#include "KnxLighting.h"      // KnxLight, KnxDimmLight, KnxRGB
#include "KnxCovers.h"        // KnxBlind
#include "KnxClimate.h"       // KnxTemperature, KnxHumidity
#include "KnxDateTime.h"      // KnxTime, KnxDate, KnxDateTime
#include "KnxScalars.h"       // KnxPercent, KnxChar, KnxFloat

/**
 * @brief Your device on the KNX bus. Create one per sketch from this device's physical address;
 *        it drives the transceiver for you and every device object (KnxLight, KnxBlind, …) is
 *        attached to it. Call begin() once in setup() and loop() every iteration.
*/
class KonnextraKNX : public KnxCoordinator {
	private:
		KnxDriver driverImpl;

	public:
#ifdef KNX_DEFAULT_PORT
		/**
		 * @brief Creates the bus connection on this board's default KNX port.
		 * @details The port is `Serial1` on most boards, and begin() opens it at 19200 8E1.
		 *          Boards whose only serial port is the console — the Uno, for instance —
		 *          do not have this constructor; name a port instead. Override the default
		 *          with `-DKNX_DEFAULT_PORT=Serial2`.
		 * @param physicalAddress This device's KNX physical address as "area.line.device",
		 *                        e.g. "1.1.5".
		 * @param chip            The bus coupler chip on that port: KnxTransceiver::Ncn5130
		 *                        (the default, KONNEXTRA BCU light) or KnxTransceiver::TpUart2.
		*/
		explicit KonnextraKNX(const String& physicalAddress,
		                      KnxTransceiver chip = KnxTransceiver::Ncn5130)
			: KnxCoordinator(&driverImpl, physicalAddress), driverImpl(physicalAddress, chip) {}
#else
		// This board has no hardware UART free for KNX — its only port is the console.
		// Name the port instead:  KonnextraKNX knx("1.1.5", Serial);
		explicit KonnextraKNX(const String& physicalAddress,
		                      KnxTransceiver chip = KnxTransceiver::Ncn5130) = delete;
#endif

		/**
		 * @brief Creates the bus connection on a serial port you name.
		 * @details begin() opens the port at 19200 8E1, so it must not be opened beforehand
		 *          unless you want to keep your own pin assignment.
		 * @param physicalAddress This device's KNX physical address, e.g. "1.1.5".
		 * @param port            The serial port the transceiver is wired to, e.g. `Serial1`.
		 * @param chip            The bus coupler chip on that port: KnxTransceiver::Ncn5130
		 *                        (the default, KONNEXTRA BCU light) or KnxTransceiver::TpUart2.
		*/
		KonnextraKNX(const String& physicalAddress, HardwareSerial& port,
		             KnxTransceiver chip = KnxTransceiver::Ncn5130)
			: KnxCoordinator(&driverImpl, physicalAddress), driverImpl(physicalAddress, port, chip) {}

		/**
		 * @brief Creates the bus connection on a stream you have already opened yourself.
		 * @details begin() leaves the line settings untouched on this path, so the stream must
		 *          already run at 19200 8E1 — KNX does not work at any other setting. Use this
		 *          for ports the library cannot configure, such as a software serial.
		 * @param physicalAddress This device's KNX physical address, e.g. "1.1.5".
		 * @param stream          An open stream connected to the transceiver.
		 * @param chip            The bus coupler chip on that port: KnxTransceiver::Ncn5130
		 *                        (the default, KONNEXTRA BCU light) or KnxTransceiver::TpUart2.
		*/
		KonnextraKNX(const String& physicalAddress, Stream& stream,
		             KnxTransceiver chip = KnxTransceiver::Ncn5130)
			: KnxCoordinator(&driverImpl, physicalAddress), driverImpl(physicalAddress, stream, chip) {}

		/**
		 * @brief Connects the bus coupler's reset and save pins, if you wired them. Call before begin().
		 * @details Both pins are optional; the library works without them. With them it waits
		 *          at begin() until the bus coupler is ready, stops sending while it is in reset,
		 *          brings it up again automatically when it comes back, and can warn you before
		 *          the bus power goes (see onBusPowerFail()).
		 *
		 *          The two chips use the reset pin differently:
		 *          - **NCN5130** (RESETB): an *output* of the chip. It goes low when the bus
		 *            voltage is too low, the chip is too hot or its watchdog fired. The library
		 *            only reads it — it cannot reset the NCN5130 through this pin.
		 *          - **TP-UART2** (RESn): an input *and* output. The library additionally pulls
		 *            it low for 2 ms at begin() and reset() to hard-reset the chip, then releases it.
		 *
		 *          Both pins are open-drain with a pull-up inside the chip, to the chip's own
		 *          logic supply (3.3 V on the NCN5130). Connect them directly to a 3.3 V board;
		 *          on a 5 V board use a level shifter. The library never drives either pin high
		 *          and enables no pull-up of its own. Without reset pin the library still
		 *          notices when the chip reset itself and brings it up again.
		 *
		 *          Where the board pin can raise an interrupt, the library uses one, so even a
		 *          short pulse is never missed; the reaction itself (your callback, the new
		 *          bring-up) runs in loop(), not in the interrupt. Pins without interrupt
		 *          capability are polled in loop(). Only one KonnextraKNX per sketch can use
		 *          status pins.
		 * @param resetPin Board pin wired to RESETB (NCN5130) or RESn (TP-UART2); -1 if not wired.
		 * @param savePin  Board pin wired to SAVEB (NCN5130) or SAVE (TP-UART2); -1 if not wired.
		*/
		void setStatusPins(int resetPin, int savePin = -1) { driverImpl.setStatusPins(resetPin, savePin); }

		/**
		 * @brief Calls your function once when the save pin reports that the bus power is failing.
		 * @details Needs a save pin (setStatusPins()). The warning comes a few milliseconds
		 *          before the bus coupler stops: enough to store a value, not for long work.
		 *          The pin change is caught by interrupt, but your function is called from
		 *          loop() — so it may use Serial, flash or EEPROM safely. Keep loop() fast.
		 * @param callback A function without arguments, e.g. `void onPowerFail() { ... }`;
		 *                 nullptr removes it.
		*/
		void onBusPowerFail(void (*callback)(void)) { driverImpl.onBusPowerFail(callback); }

		/**
		 * @brief Whether the bus power is good, according to the save pin.
		 * @return false while the save pin is low; always true if no save pin is connected.
		*/
		bool busPowerOk(void) const { return driverImpl.busPowerOk(); }
};
