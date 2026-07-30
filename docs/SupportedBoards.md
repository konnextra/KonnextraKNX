# Supported Boards {#boards}

This is an Arduino-framework library, and it should build on **any board the Arduino
framework supports**. Nothing in it is tied to a chip: the KNX transceiver is reached through
a serial port you name, and every layer above that is plain portable C++. There is not a
single architecture check in the driver.

"Should" is doing real work in that sentence, so the rest of this page is about the
difference between *should build*, *does build*, and *has actually run on a bus*.

## Built on every push

Each of these compiles all four example sketches on every commit. One board per Arduino
**core family**, because a core family is what actually differs — two boards sharing a core
will behave the same way.

| Board | Core family | Notes |
|---|---|---|
| Seeed XIAO ESP32-C6 | ESP32 | the reference board |
| ESP32-S3-DevKitC-1 | ESP32 | a second variant, without the XIAO pin aliases |
| Arduino Mega 2560 | AVR (classic) | `Serial1`…`Serial3` free |
| Arduino Giga R1 | Arduino mbed / ArduinoCore-API | the family SAMD, Nano 33 and Portenta share |
| Arduino UNO R4 Minima | Renesas RA | |
| ST Nucleo F401RE | STM32duino | needs `-DENABLE_HWSERIAL1`, see below |
| Raspberry Pi Pico | RP2040, Earle Philhower core | |
| Arduino Uno | AVR (classic) | only the explicit-port sketch — see below |

If your board is not listed but shares a core family with one that is, it will almost
certainly work.

## How the serial port is chosen

Written without a port, the node uses `Serial1`:

```cpp
Konnextra knx("1.1.5");            // Serial1
Konnextra knx("1.1.5", Serial2);   // or name your own
```

`Serial1` is the convention for "the first hardware UART that is not the USB console", which
is why it is the default. The library resolves it in this order:

1. `SERIAL_PORT_HARDWARE_OPEN`, if your core defines it — this is Arduino's own answer to the
   same question
2. `HAVE_HWSERIAL1`, which AVR and STM32duino both set when `Serial1` really exists
3. otherwise `Serial1`, on cores that always provide it

Override it for a whole project without touching the sketch:

```ini
build_flags = -DKNX_DEFAULT_PORT=Serial2
```

## The Uno problem

The Uno has **one** hardware UART, and the USB connection shares it. There is no second port
to give KNX.

The library does not pretend otherwise. On such a board the one-argument constructor does not
exist at all, and using it is a compile error rather than a device that mysteriously fails:

```
error: use of deleted function 'Konnextra::Konnextra(const String&)'
```

You can still use the Uno — name the port explicitly:

```cpp
Konnextra knx("1.1.5", Serial);
```

But understand the trade: `Serial` now belongs to KNX. **There is no serial monitor**, so no
`Serial.println()` debugging and no tracing output, and you must unplug the transceiver to
upload a sketch. It works, and it is a poor place to develop.

Software serial does not rescue this. The library bundled with the AVR core cannot produce
even parity, and KNX needs 8E1 — a software port would look configured and silently mangle
every frame.

The same applies to any single-UART AVR board: **Uno, Nano, Pro Mini, Duemilanove**. Boards
with a separate USB chip are fine — the Leonardo and Micro keep `Serial` on USB and leave
`Serial1` free. If you want an Uno form factor without the problem, the **UNO R4 Minima** has
`Serial1` on pins 0/1 and a separate USB port.

## Per-platform notes

**ESP32** — the UART starts on the core's default pins, which are almost certainly not where
you wired the transceiver. Assign them before `begin()`:

```cpp
HardwareSerial knxPort(1);
Konnextra      knx("1.1.5", knxPort);

void setup() {
    knxPort.setPins(rxPin, txPin);
    knx.begin();
}
```

The library keeps whatever assignment the port already has, so this survives `begin()`.

**STM32duino** — `Serial1` is declared whenever the chip has a USART1 but only created when
you ask for it. Without `-DENABLE_HWSERIAL1` you get `undefined reference to 'Serial1'` at
link time. Add the flag, or name a port that does exist.

**AVR** — needs a spare UART, so a Mega, Leonardo or Micro rather than an Uno. Note that
`loop()` has to keep running: at 16 MHz there is less headroom for anything that blocks.

## If your board is not listed

Try it. If it builds and runs, tell us and it goes in the table. If it does not build, the
error almost always names the missing piece — @ref troubleshooting covers the two that come
up most, and neither is hard to fix.
