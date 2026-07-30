#pragma once
/**
 * @name Konnextra.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details The only header a sketch includes. It brings in the whole KNX library — the bus node,
 *          the value types (KnxValue and the Dpt*() factories), and every device object (KnxLight,
 *          KnxBlind, KnxTemperature, …) — so a sketch needs just:
 *
 *              #include <Konnextra.h>
 *              Konnextra knx("1.1.5");                   // this device's KNX address
 *              KnxLight kitchen(knx, "0/1/1", "0/3/0");
*/

//---- Bus driver ----
#include "KnxDriver.h"

//---- Bus node + value types ----
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
class Konnextra : public KnxCoordinator {
	private:
		KnxDriver driverImpl;

	public:
#ifdef KNX_DEFAULT_PORT
		/**
		 * @brief Creates the bus node on this board's default KNX port.
		 * @details The port is `Serial1` on most boards, and begin() opens it at 19200 8E1.
		 *          Boards whose only serial port is the console — the Uno, for instance —
		 *          do not have this constructor; name a port instead. Override the default
		 *          with `-DKNX_DEFAULT_PORT=Serial2`.
		 * @param physicalAddress This device's KNX physical address as "area.line.device",
		 *                        e.g. "1.1.5".
		*/
		explicit Konnextra(const String& physicalAddress)
			: KnxCoordinator(&driverImpl, physicalAddress), driverImpl(physicalAddress) {}
#else
		// This board has no hardware UART free for KNX — its only port is the console.
		// Name the port instead:  Konnextra knx("1.1.5", Serial);
		explicit Konnextra(const String& physicalAddress) = delete;
#endif

		/**
		 * @brief Creates the bus node on a serial port you name.
		 * @details begin() opens the port at 19200 8E1, so it must not be opened beforehand
		 *          unless you want to keep your own pin assignment.
		 * @param physicalAddress This device's KNX physical address, e.g. "1.1.5".
		 * @param port            The serial port the transceiver is wired to, e.g. `Serial1`.
		*/
		Konnextra(const String& physicalAddress, HardwareSerial& port)
			: KnxCoordinator(&driverImpl, physicalAddress), driverImpl(physicalAddress, port) {}

		/**
		 * @brief Creates the bus node on a stream you have already opened yourself.
		 * @details begin() leaves the line settings untouched on this path, so the stream must
		 *          already run at 19200 8E1 — KNX does not work at any other setting. Use this
		 *          for ports the library cannot configure, such as a software serial.
		 * @param physicalAddress This device's KNX physical address, e.g. "1.1.5".
		 * @param stream          An open stream connected to the transceiver.
		*/
		Konnextra(const String& physicalAddress, Stream& stream)
			: KnxCoordinator(&driverImpl, physicalAddress), driverImpl(physicalAddress, stream) {}
};
