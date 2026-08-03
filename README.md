# Konnextra KNX

**Talk to a KNX bus from an Arduino sketch — no ETS, no KNX-stack expertise.**

> **🚧 Under development.** The library works and is tested on real hardware, but the API is
> still moving and things will change before 1.0. The Konnextra Bridge board and the Konnextra
> website both launch shortly.

KNX is the building-automation standard behind a lot of professional lighting, blind and
climate installations. Getting a microcontroller onto that bus normally means an ETS project,
a stack of datapoint tables and a lot of timing-critical code. This library removes all three:
you describe devices by **what they are**, and it handles the datapoints, framing and bus
timing for you.

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

That is a complete, working sketch.

## What makes it different

- **You write intent, not protocol.** `lamp.on()`, `blind.down()`, `light.brighter()` — not
  datapoint identifiers and bit-packed payloads. A dozen device types cover the common cases,
  and a generic `KnxObject` covers everything else.
- **Wrong values don't reach the bus.** Each `Dpt*()` factory accepts only the data its
  datapoint actually carries, so a type mistake is a compile error rather than a malformed
  telegram in a live installation.
- **Sends tell you the truth.** Every command returns whether the bus *confirmed* it — the
  transceiver's real `L_Data.con`, not an optimistic `true`.
- **Nothing blocks your sketch.** No timing-critical interrupt handlers run on your
  microcontroller: the bit-level KNX timing lives on the board's co-processor. WiFi, MQTT or
  Matter can share the same chip without breaking bus communication.
- **No ETS.** Group addresses are written directly in your sketch. Nothing to configure, no
  project file, no licence.
- **The core is tested without hardware.** Framing, datapoint encoding and dispatch are
  hardware-independent and covered by a host test suite that runs on every push.

## Hardware

Designed for the **Konnextra Bridge for Arduino and KNX** — an STKNX breakout board with a
galvanically isolated ATtiny co-processor:

| | |
|---|---|
| Transceiver | STMicroelectronics STKNX |
| Co-processor | Microchip ATtiny1616 |
| Isolation | Texas Instruments ISO7721DR digital isolator |
| Host voltage | 2.25 V – 5.5 V (UART) |
| Dimensions | 27.3 × 31.9 mm, 2× M2 mounting holes |

The ATtiny handles the KNX physical layer — bit timing, collision detection and frame
buffering — and presents a plain UART to your microcontroller. The isolator keeps the bus and
your board electrically separate.

That UART speaks the **TP-UART2 protocol**, so the library is not limited to this board — it
drives a standard TP-UART2 transceiver just as well, and is tested against both. The link runs
at 19200 baud, 8E1, with the usual `U_Reset.req` / `U_State.req` / `U_SetAddress` command set
and real `L_Data.con` confirmations.

## Boards

Built on the **Arduino framework** and usable from any Arduino-compatible board. Every push
compiles the examples against AVR, Renesas, STM32duino, RP2040 and ESP32 — one job per core
family. Development and bench testing happen on the **Seeed XIAO ESP32-C6**.

The node uses your board's default KNX port, `Serial1` on almost every board, and opens it at
19200 8E1 for you. Name a different one when the transceiver sits elsewhere:

```cpp
Konnextra knx("1.1.5", Serial2);
```

A board whose only serial port is the USB console — the Uno — has no default: name the port
there, and the serial monitor goes with it.

## Examples

**A device object** — the usual way. Create one per thing on the bus, command it by name, get
a callback when it changes:

```cpp
Konnextra knx("1.1.5");
KnxLight  lamp(knx, "0/1/1", "0/3/0");

void setup() {
    knx.begin();
    lamp.onUpdate(onLampChanged);   // fires when the lamp changes on the bus
}

void loop() {
    knx.loop();
}

void onLampChanged(bool on) {
    // react to the light's new state
}
```

**A one-off send** — no object, just a typed value to a group address:

```cpp
knx.send("0/1/1", Dpt1(true));      // an on/off value
knx.send("0/4/2", Dpt9(21.5f));     // a temperature
```

**Any other datapoint** — the generic object takes the type once, then sends and receives it:

```cpp
KnxObject counter(knx, "0/5/0", KnxDpt::DPT7);   // 16-bit unsigned

counter.write(Dpt7(1000));
counter.onUpdate([](const KnxValue& v) { uint16_t n = v.asU16(); });
```

Device types available out of the box: `KnxLight`, `KnxDimmLight`, `KnxRGB`, `KnxBlind`,
`KnxTemperature`, `KnxHumidity`, `KnxPercent`, `KnxTime`, `KnxDate`, `KnxDateTime`, `KnxChar`,
`KnxFloat`, plus `KnxObject` for anything else.

Full versions of these three sketches are on the [Examples
page](https://konnextra.github.io/KonnextraKNX/examples.html).

## Installing

**PlatformIO** — add to your `platformio.ini`:

```ini
lib_deps =
    https://github.com/konnextra/KonnextraKNX.git
```

**Arduino IDE** — download the library and add it through *Sketch → Include Library → Add .ZIP
Library…*, then `#include <Konnextra.h>`.

## Documentation

- **[Getting Started](https://konnextra.github.io/KonnextraKNX/)** — the guided introduction:
  addresses, device objects, callbacks, raw sends, debugging.
- **[API reference](https://konnextra.github.io/KonnextraKNX/annotated.html)** — every class
  and method.
- **[konnextra.at](https://konnextra.at)** — the boards, including the aux-power supply and
  the multi-sensor extension.

Something not behaving? Turn on tracing and watch every telegram go by:

```cpp
knx.enableDebugMode(true);   // call before begin() to trace start-up too
```

## Licence

[BSD 3-Clause](LICENSE). Use it, change it, ship it in a commercial product. Keep the copyright
notice, and do not use the Konnextra name to advertise something you built from it.
