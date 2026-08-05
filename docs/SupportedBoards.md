# Supported Boards {#boards}

This is an Arduino-framework library, and it should build on **any board the Arduino framework
supports**. Nothing in it is tied to a chip. The KNX transceiver is reached through a serial
port you name, and every layer above that is plain portable C++. There is not a single
architecture check in the driver.

"Should" is doing real work in that sentence, so the rest of this page is about the difference
between *should build*, *does build*, and *has actually run on a bus*.

## Built on every push

Each of these compiles all four example sketches on every commit. One board per Arduino **core
family**, because a core family is what actually differs. Two boards sharing a core will behave
the same way.

| Board | Core family | Notes |
|---|---|---|
| Seeed XIAO ESP32-C6 | ESP32 | the reference board |
| ESP32-S3-DevKitC-1 | ESP32 | a second variant, without the XIAO pin aliases |
| Arduino Mega 2560 | AVR (classic) | `Serial1`…`Serial3` free |
| Arduino Giga R1 | Arduino mbed / ArduinoCore-API | the family SAMD, Nano 33 and Portenta share; **defaults to `Serial2`, not `Serial1`** |
| Arduino UNO R4 Minima | Renesas RA | |
| ST Nucleo F401RE | STM32duino | needs `ENABLE_HWSERIAL1`, see below |
| Raspberry Pi Pico | RP2040, `arduino-pico` core | |
| Arduino Uno | AVR (classic) | only the explicit-port sketch, see below |

If your board is not listed but shares a core family with one that is, it will almost certainly
work.

## Run on real hardware

Compiling is not the same as working. These eight boards have been on a bench with the library
actually running on them:

| Board | Core family | Port it used |
|---|---|---|
| Seeed XIAO ESP32-C6 | ESP32 | UART1, on pins the sketch assigns |
| NodeMCU-32S (ESP32-WROOM-32) | ESP32 | `Serial1`, GPIO 26 and 27 |
| ST Nucleo-L432KC | STM32duino | `Serial1`, PA9 and PA10 |
| Arduino UNO R4 Minima | Renesas RA | `Serial1`, D0 and D1 |
| Arduino GIGA R1 | Arduino mbed | `Serial2`, D18 and D19 |
| Arduino Mega 2560 | AVR (classic) | `Serial1`, D18 and D19 |
| Arduino Uno R3 | AVR (classic) | `Serial`, named by hand |
| Raspberry Pi Pico 2 | `arduino-pico` | `Serial1`, GP0 and GP1 |

On each of them the serial line was captured while the library sent telegrams, and the bytes
were compared against the other boards. All eight produced the same telegram, down to the
checksum, so framing, addressing and the line settings behave identically across every core
family above. The **Seeed XIAO ESP32-C6** additionally runs against a real TP-UART2 transceiver
on a live bus, which is the board the library is developed on day to day.

Two of these boards are worth reading the notes for before you buy one: the **GIGA R1** uses
`Serial2` rather than `Serial1`, and the **Uno R3** cannot use the address-only constructor at
all. Both are covered below.

## How the serial port is chosen

This is the one thing you may have to think about per board. Written without a port, the
library asks your board's core which UART is free:

```cpp
Konnextra knx("1.1.5");            // your board's free UART, usually Serial1
Konnextra knx("1.1.5", Serial2);   // or name your own
```

On almost every board the answer is `Serial1`, the convention for "the first hardware UART that
is not the USB console". **The Arduino Giga R1 is the exception: it answers `Serial2`**, so a
sketch written without a port talks on the Giga's `Serial2` pins and wiring to `Serial1` gets
you nothing. If you are unsure which port your board picked, name one explicitly — that always
wins.

The library resolves it in this order:

1. `SERIAL_PORT_HARDWARE_OPEN`, if your core defines it. This is Arduino's own answer to the
   same question.
2. `HAVE_HWSERIAL1`, which AVR and STM32duino both set when `Serial1` really exists.
3. otherwise `Serial1`, on cores that always provide it.

Either way, `begin()` opens that port at 19200 8E1. You do not open it yourself.

To override the default for a whole project without touching the sketch, in PlatformIO:

```ini
build_flags = -DKNX_DEFAULT_PORT=Serial2
```

That is a `platformio.ini` setting. The Arduino IDE has no equivalent, so there you name the
port in the sketch instead, which does the same thing.

### A port the library cannot configure

For a software serial, or any port that is not a `HardwareSerial`, open it yourself and pass it
in. `begin()` leaves the line settings alone on this path, so they have to be right:

```cpp
EspSoftwareSerial::UART knxPort;
Konnextra               knx("1.1.5", knxPort);

void setup() {
    knxPort.begin(19200, SWSERIAL_8E1, 4, 5);
    knx.begin();
}
```

The software serial bundled with the AVR core cannot produce even parity at all, so it will not
work for KNX no matter how it is configured.

## The Uno problem

The Uno has **one** hardware UART, and the USB connection shares it. There is no second port to
give KNX.

The library does not pretend otherwise. On such a board the one-argument constructor does not
exist at all, and using it is a compile error rather than a device that mysteriously fails:

```
error: use of deleted function 'Konnextra::Konnextra(const String&)'
```

You can still use the Uno by naming the port explicitly:

```cpp
Konnextra knx("1.1.5", Serial);
```

But understand the trade. `Serial` now belongs to KNX, so **there is no serial monitor**. No
`Serial.println()` debugging, and you have to unplug the transceiver to upload a sketch. It
works, and it is a poor place to develop.

Tracing is worse than merely unavailable here: `enableDebugMode(true)` writes its `[knx]` lines
to `Serial` too, which on this board *is* the KNX line. The log text would go to the transceiver
interleaved with real telegrams. **Leave debug mode off on a single-UART board.**

The same applies to any single-UART AVR board: **Uno, Nano, Pro Mini, Duemilanove**. Boards with
a separate USB chip are fine, the Leonardo and Micro keep `Serial` on USB and leave `Serial1`
free. If you want an Uno form factor without the problem, the **UNO R4 Minima** has `Serial1` on
pins 0/1 and a separate USB port.

## Per-platform notes

**ESP32**, the UART starts on the core's default pins, which are almost certainly not where you
wired the transceiver. Assign them before `begin()`:

```cpp
HardwareSerial knxPort(1);
Konnextra      knx("1.1.5", knxPort);

void setup() {
    knxPort.setPins(rxPin, txPin);
    knx.begin();
}
```

The library keeps whatever assignment the port already has, so this survives `begin()`.

**STM32duino**, `Serial1` is declared whenever the chip has a USART1 but only created when you
ask for it. Without `ENABLE_HWSERIAL1` defined you get `undefined reference to 'Serial1'` at
link time. Define it, or name a port that does exist.

**AVR**, needs a spare UART, so a Mega, Leonardo or Micro rather than an Uno. Note that `loop()`
has to keep running: at 16 MHz there is less headroom for anything that blocks.

## If your board is not listed

Try it. If it builds and runs, tell us and it goes in the table. If it does not build, the error
almost always names the missing piece. @ref troubleshooting covers the two that come up most,
and neither is hard to fix.
