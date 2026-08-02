# Getting Started

Talk to a KNX bus from an Arduino sketch in a few lines. No ETS, no KNX-stack expertise.
You describe things by what they are, a light, a blind, a temperature, and the library
handles the datapoints, framing and bus timing.

```cpp
#include <Konnextra.h>

Konnextra knx("1.1.5");                 // this device's KNX address
KnxLight  lamp(knx, "0/1/1", "0/3/0");  // command address, status address

void setup() {
    knx.begin();
}

void loop() {
    knx.loop();
    lamp.toggle();      // flips the lamp on the bus
    delay(5000);
}
```

That is a complete, working sketch. Read on to receive status, use other device types, and
send raw values.

---

## What you need

- A board the Arduino framework supports. @ref boards has the list and the one board that
  cannot work.
- The **%Konnextra Bridge**, an STKNX breakout board that connects your board to the KNX bus.
  Any TP-UART2 module works too.
- This library, added through PlatformIO or the Arduino IDE.

Each device on a KNX bus has a **physical address** like `1.1.5` that identifies the hardware.
Devices talk to each other through **group addresses** like `0/1/1` that identify a topic, for
example "the kitchen light". You set the physical address once, and give each device object the
group address it uses. @ref knxbasics explains both, and how to find out which group addresses
your installation uses.

## Wiring

Three connections between your board and the transceiver: **RX**, **TX** and a shared **GND**.
The transceiver draws its power from the KNX bus. On most boards the library uses `Serial1`,
so that is where the two data wires go. @ref hardware has the details.

## Installing

**PlatformIO**, add the library to your `platformio.ini`:

```ini
lib_deps =
    https://github.com/konnextra/KonnextraKNX.git
```

**Arduino IDE**, download the library and add it through
*Sketch → Include Library → Add .ZIP Library…*, then `#include <Konnextra.h>`.

## The knx object

Everything starts with one `Konnextra`, created from this device's physical address. It is
your sketch's connection to the bus. Call `begin()` once in `setup()`, and `loop()` on every
pass of your sketch's `loop()` so the library can receive telegrams:

```cpp
Konnextra knx("1.1.5");

void setup() {
    knx.begin();
}

void loop() {
    knx.loop();     // never block this with long delay()s
}
```

> `loop()` must run often. Do not gate it behind a long `delay()`, or incoming status updates
> will arrive late or be missed. Use `millis()` for timing instead.

`begin()` also opens the serial port the transceiver is on, at 19200 8E1. You do not open it
yourself. Written as above, the library picks your board's default port. To name a different
one, or to find out why the Uno needs special treatment, see @ref boards.

## Device objects

A device object is created once, usually as a global, from the group address it uses. It sends
commands, remembers the last value it saw, and calls a function you provide when the value
changes on the bus.

```cpp
Konnextra knx("1.1.5");
KnxLight  lamp(knx, "0/1/1", "0/3/0");   // sends on 0/1/1, reads status on 0/3/0

void onLampChanged(bool on);             // declared here, defined below

void setup() {
    knx.begin();
    lamp.onUpdate(onLampChanged);        // called whenever the lamp changes
}

void loop() {
    knx.loop();
}

void onLampChanged(bool on) {
    // react to the new state
}
```

Commanding the light:

```cpp
lamp.on();          // switch on
lamp.off();         // switch off
lamp.toggle();      // flip, tracking the real bus state
bool state = lamp.isOn();   // last known state, no bus traffic
```

Most device objects take a **command** group address and a **status** group address. Pass one
address to use it for both. Every command returns `true` if the bus confirmed it.

### Available device objects

| Object | For |
|---|---|
| `KnxLight` | a switchable light |
| `KnxDimmLight` | a light that also dims (brighter / darker / stop) |
| `KnxRGB` | an RGB colour light |
| `KnxBlind` | a blind or roller shutter (up / down / stop) |
| `KnxTemperature`, `KnxHumidity` | publish or read a climate value |
| `KnxTime`, `KnxDate`, `KnxDateTime` | publish or read date and time |
| `KnxPercent` | a 0–100 % value |
| `KnxChar` | a single character |
| `KnxFloat` | a 32-bit number |
| `KnxObject` | any other datapoint, you pick the type |

Each is documented on its own page in the reference, with its constructors and methods.

## Sending a raw value

For a one-off send to any group address, without keeping a device object, use `knx` directly
with a typed value:

```cpp
knx.send("0/4/2", Dpt9(21.5f));     // send 21.5 as a floating-point value
knx.send("0/1/1", Dpt1(true));      // send an on/off value
```

`Dpt1()`, `Dpt9()`, `Dpt232()` and the rest each accept only the data their datapoint expects,
so a wrong value type is caught by the compiler instead of being sent as a bad telegram. This
path only sends. Use a device object when you need to receive.

## Diagnosing a problem

Turn on verbose tracing to see every telegram the library sends and receives, printed over
Serial with a `[knx]` prefix:

```cpp
knx.enableDebugMode(true);   // call before begin() to trace start-up too
```

Leave it off in normal operation. The printing is chatty and slows the receive path. Turn it
on to explain a run that is misbehaving.

## Next

- @ref knxbasics if the addressing is new to you, including how to find out which group
  addresses your installation already uses.
- @ref examples for complete sketches, and ideas for what to build.
- @ref datapoints when you need a value type that has no ready-made device object.
- @ref boards before you order hardware, or when the default serial port is not where your
  transceiver is.
- @ref troubleshooting when something does not work, and @ref faq for the questions that come
  up most.

Full class and method documentation is in the **API reference**.
