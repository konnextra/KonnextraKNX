# Release Notes {#releasenotes}

Changes that affect your sketch. Releases before 0.1.7 predate the current API and are not
listed.

## 0.1.8

**Breaking: the library and its class are now `KonnextraKNX`.** Change `#include <Konnextra.h>`
to `#include <KonnextraKNX.h>`, and `Konnextra knx(...)` to `KonnextraKNX knx(...)`, in every
sketch. Nothing else about the API changed.

**Installs from the Arduino IDE.** The Library Manager and "Add .ZIP Library" both rejected the
repository before, because it carried no library manifest at its root. It now has one, and the
flat `src/` layout the IDE expects. If you point PlatformIO's `lib_deps` straight at the
repository URL, the dependency may resolve differently.

**Eight boards have actually run it.** @ref boards now lists the boards the library was tested
on, not just the ones CI compiles: XIAO ESP32-C6, ESP32-WROOM-32, Nucleo-L432KC, UNO R4 Minima,
GIGA R1, Mega 2560, Uno R3 and Pico 2.

**The Giga's default port is `Serial2`.** The library always picked it, because it asks the core
which UART is free, but the documentation said `Serial1`. A Giga sketch written without a port
talks on D18/D19.

**Licensed under BSD 3-Clause.** Use it, change it and ship it inside a commercial product. Keep
the copyright notice, and do not advertise a derived product with the Konnextra name.

## 0.1.7

**The serial port is now yours to choose.** The library no longer builds its own UART on fixed
pins. Written without a port it uses `Serial1`, or whichever port your core reports as free, and
you can also name one yourself. @ref boards has the details.

*Breaking on ESP32.* Earlier versions hard-wired the transceiver to D6/D7. Those pins are no
longer set for you, so assign them before `begin()` with `setPins()`. The ESP32 section of
@ref boards shows how.

**Runs on any Arduino core.** AVR, Renesas, STM32duino, RP2040, mbed and ESP32 are built on
every commit. That includes the Uno, which has no spare port and where the one-argument
constructor is now a compile error instead of a silent failure.

**No more reset line.** The transceiver's `/RESET` pin is gone from the hardware, so a soft reset
that goes unanswered now fails `begin()` instead of falling back.

**`KnxDpt::DPT16` removed.** It was in the enum but never implemented, nothing could encode or
decode it.

**Documentation:** new pages for @ref knxbasics, @ref datapoints, @ref boards,
@ref troubleshooting, @ref faq and @ref howitworks.
