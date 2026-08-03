# Examples {#examples}

Three sketches, from the most convenient way to talk to the bus to the most general.

## Device object

A device object is the usual way. Create one for each thing on the bus, command it with named
methods, and register a callback for status changes. Here a light is switched and its status is
tracked.

```cpp
#include <Konnextra.h>

Konnextra knx("1.1.5");                 // this device's KNX address
KnxLight  lamp(knx, "0/1/1", "0/3/0");  // command address, status address

void onLampChanged(bool on);            // defined below

unsigned long lastToggle = 0;

void setup() {
    knx.begin();
    lamp.onUpdate(onLampChanged);       // called when the lamp changes on the bus
}

void loop() {
    knx.loop();                         // receives telegrams, fires callbacks

    // Toggle every 5 s. Never use delay() for this: it stalls knx.loop(),
    // so status telegrams arrive late or are missed entirely.
    if (millis() - lastToggle >= 5000) {
        lastToggle = millis();
        lamp.toggle();                  // flips relative to the real bus state
    }
}

void onLampChanged(bool on) {
    // react to the light's new state
}
```

### What the object actually knows

`lamp.isOn()` returns the last state the object heard about, with no bus traffic. That makes it
cheap to call, but it is worth being precise about where that state comes from:

- **After a status telegram arrives** on `0/3/0`, it is the real state of the light, whoever
  switched it. A wall switch, a timer, another sketch.
- **Right after you send a command**, the object assumes the command worked, so `isOn()` changes
  immediately. A status telegram then confirms or corrects it.
- **At start-up it knows nothing.** Until the first status telegram arrives, `isOn()` returns
  `false`, whatever the light is really doing. The library cannot ask, it can only listen. If
  that matters for your sketch, have the actuator send its status cyclically, or wait for the
  first callback before acting on the value.

`lamp.toggle()` flips relative to that same known state, so a freshly booted sketch may toggle
the "wrong" way once and then be in step.

### Which objects exist

There are ready-made objects for the things people use most: lights, dimmers, RGB, blinds,
temperature and humidity, time and date, percentages, plain numbers. Getting Started lists them
all. More will be added.

If nothing fits what you want to talk to, that is not a dead end. Use **Custom KnxObject**
below, which works with any datapoint type.

## Stateless send

To send a one-off value to any group address without keeping an object, use `knx` directly with
a typed value. There is no status callback on this path. It only sends.

```cpp
#include <Konnextra.h>

Konnextra knx("1.1.5");

void setup() {
    knx.begin();
    knx.send("0/1/1", Dpt1(true));      // an on/off value
    knx.send("0/4/2", Dpt9(21.5f));     // a temperature, as a floating-point value
}

void loop() {
    knx.loop();
}
```

`Dpt1()` and `Dpt9()` wrap a plain C++ value into a KNX value of that datapoint type. Each one
accepts only the data its datapoint can carry, so `Dpt1(21.5f)` does not compile. A wrong value
type is a build error instead of a bad telegram on the bus.

## Custom KnxObject

For a datapoint that has no dedicated class, use the generic `KnxObject`. Give it the group
address and the datapoint type once, then send with `write()` and receive with `onUpdate()`.
Here a 16-bit counter (DPT 7) is published and read.

```cpp
#include <Konnextra.h>

Konnextra knx("1.1.5");
KnxObject counter(knx, "0/5/0", KnxDpt::DPT7);   // 16-bit unsigned value

void onCounter(const KnxValue& value);

void setup() {
    knx.begin();
    counter.onUpdate(onCounter);
    counter.write(Dpt7(1000));          // send a value
}

void loop() {
    knx.loop();
}

void onCounter(const KnxValue& value) {
    uint16_t count = value.asU16();     // read the received value as its type
}
```

`KnxObject` hands your callback a generic ::KnxValue. Read it with the accessor for its type
(`asU16()`, `asFloat()`, `asBool()`, …). @ref datapoints lists which accessor goes with which
type.

If you find yourself writing the same `KnxObject` calls over and over, you can wrap them in a
class of your own with named methods, the same way `KnxLight` is built. @ref howitworks shows
how.

## Ideas

Nothing here is exotic. Each one is a short sketch on top of what is above.

- **A wall controller.** Touch pads or buttons switching lights and blinds, with an OLED showing
  the state that comes back from the bus.
- **KNX on your phone.** An ESP32 bridging the bus to MQTT or Home Assistant, so an installation
  with no gateway gets one for the price of a dev board.
- **Sensors the installation is missing.** Publish a temperature, humidity or CO₂ reading to a
  group address and every KNX display in the building can show it.
- **Presence and light.** A cheap PIR or lux sensor driving KNX lights, without buying a KNX
  sensor for each room.
- **A scene button.** One press, several telegrams: lights down, blinds closed, heating back.
- **A bus watcher.** Log every telegram to an SD card or to a web page. Useful for finding out
  what an unfamiliar installation is doing, see @ref knxbasics.
- **Whatever else has a group address.** Anything on the bus is reachable the same way.

The sketches above live in the `examples/` folder of the repository. There is a fourth one
there, `ExplicitPort`, for boards where the transceiver is not on the default serial port.
@ref boards explains when you need it.
