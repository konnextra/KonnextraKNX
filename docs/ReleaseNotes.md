# Release Notes {#releasenotes}

Changes that affect your sketch. Releases before 0.1.7 predate the current API and are not
listed.

## 0.1.7 — unreleased

**The serial port is now yours to choose.** The node no longer builds its own UART on fixed
pins. Written without a port it uses `Serial1`, or whichever port your core reports as free;
you can also name one:

```cpp
Konnextra knx("1.1.5");            // Serial1
Konnextra knx("1.1.5", Serial2);   // a port you name
Konnextra knx("1.1.5", knxPort);   // a HardwareSerial or Stream you set up yourself
```

*Breaking on ESP32.* Earlier versions hard-wired the transceiver to D6/D7. They are no longer
set for you — assign them before `begin()`:

```cpp
HardwareSerial knxPort(1);
Konnextra      knx("1.1.5", knxPort);

void setup() {
    knxPort.setPins(D7, D6);   // (rx, tx)
    knx.begin();
}
```

**Runs on any Arduino core.** AVR, Renesas, STM32duino, RP2040, mbed and ESP32 are built on
every commit. @ref boards has the details, including the Uno, which has no spare port and
where the one-argument constructor is now a compile error rather than a silent failure.

**No more reset line.** The transceiver's `/RESET` pin is gone from the hardware, so a soft
reset that goes unanswered now fails `begin()` instead of falling back.

**`KnxDpt::DPT16` removed.** It was in the enum but never implemented — nothing could encode
or decode it.

**Documentation:** new pages for @ref knxbasics, @ref datapoints, @ref boards,
@ref troubleshooting, @ref faq and @ref howitworks.
