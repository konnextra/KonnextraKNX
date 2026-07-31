# How It Works {#howitworks}

You do not need this page to use the library. Read it when you want to extend it: add a
device class the library does not have, drive a transceiver it does not know, or test a
sketch's logic without a bus.

## The layers

The library is a stack of small pieces, each of which only knows about the one below it.
Nothing ever reaches upwards.

| Layer | Does | You see it |
|---|---|---|
| `Konnextra` | the node you write in a sketch — a bus node that owns its driver | always |
| Device objects | `KnxLight`, `KnxTemperature`, `KnxObject`, … — a group address plus a datapoint type plus a cached value | always |
| `KnxCoordinator` | sends to group addresses, receives telegrams and hands them to the objects that want them | when you supply your own driver |
| `KnxDriver` | speaks the transceiver's serial command set and reports whether a send was confirmed | rarely |
| Framing and codec | builds and parses telegrams, converts values to and from bus bytes | no |

Two of those boundaries are interfaces rather than classes, and that is what makes the
library extensible: `IKnxDriver` below the coordinator, `IKnxReceiver` above it. The
coordinator knows neither `KnxDriver` nor `KnxLight` — only the two contracts. Swap either
side and the other does not notice.

`Konnextra` is the convenience on top: a `KnxCoordinator` that also constructs its own
`KnxDriver`, so you type the physical address once and never think about injection.

## What happens when you send

```cpp
lamp.on();
```

1. `KnxLight::on()` calls `write(Dpt1(true))` — the device class's only job is knowing that a
   light is a DPT 1.
2. `KnxObject::write()` updates its own cache optimistically, so `isOn()` is right
   immediately, and passes the value to the node.
3. The node builds a telegram: source is this device's physical address, destination the
   command group address, and the value is encoded according to its datapoint type.
4. The driver sends the frame to the transceiver and **waits for the bus to confirm it**, up
   to 100 ms.
5. That confirmation is the `true` or `false` you get back. It is never invented.

If a status telegram arrives later on the status address, it overwrites the optimistic cache
with what the bus actually reports.

## What happens when you receive

`knx.loop()` drains everything the driver has ready — it does not stop after one telegram:

1. The driver reassembles bytes from the transceiver into complete frames.
2. Each frame is parsed into source, destination, and payload. A bad checksum stops here.
3. If the telegram is addressed to a group, the node walks its list of objects and offers the
   address to each. **Every** object that recognises it gets the telegram — two objects on one
   address both fire.
4. Each recipient decodes the payload with *its own* datapoint type. This is why a type
   mismatch produces a wrong value rather than an error: the bytes decode fine, just as
   something else.
5. The cache is updated and your callback runs — inside `loop()`, on your own task. There is
   no interrupt and no separate thread anywhere in the library.

Telegrams addressed to this device individually take a different path, and are ignored unless
you install a handler for them. Telegrams for other devices are seen and dropped; with tracing
on they are logged as reaching "0 receiver(s)", which is normal.

## Writing your own device class

A device class is a thin layer over `KnxObject` that hides the datapoint type behind names
that mean something. `KnxLight` is barely sixty lines, and most of them are constructors.

The pattern is: pick the type in the constructor, wrap `write()` in named methods, and
override `onValueUpdated()` to turn the cached value into a typed callback.

```cpp
#include <Konnextra.h>

class KnxCounter : public KnxObject {
    private:
        void (*p_onChange)(uint16_t count) = nullptr;

    protected:
        void onValueUpdated(void) override {
            if (p_onChange) p_onChange(cachedValue.asU16());
        }

    public:
        KnxCounter(KnxCoordinator& knx, String ga)
            : KnxObject(knx, ga, KnxDpt::DPT7) {}

        bool set(uint16_t count) { return write(Dpt7(count)); }
        uint16_t count(void) const { return cachedValue.asU16(); }

        void onUpdate(void (*callback)(uint16_t count)) { p_onChange = callback; }
};
```

Three things worth knowing:

- `cachedValue` is `protected`, so your subclass reads it directly. It has already been
  decoded and validated by the time `onValueUpdated()` runs.
- Overriding `onValueUpdated()` *replaces* the generic callback rather than adding to it —
  the built-in classes do the same. Call `KnxObject::onValueUpdated()` from your override if
  you want both.
- Registration is automatic. The `KnxObject` constructor attaches the object to the node and
  the destructor detaches it, so an object that goes out of scope simply stops receiving.

## Writing your own driver

If your transceiver is not TP-UART-compatible, implement `IKnxDriver` and hand it to a
`KnxCoordinator` directly instead of using `Konnextra`. Four methods:

| Method | Contract |
|---|---|
| `begin()` | bring the transport up; `true` on success |
| `reset()` | reset the transceiver and re-apply its configuration |
| `sendTelegram(frame, length)` | transmit, and return whether **the bus** confirmed it — not whether the write succeeded |
| `poll(out, maxLen, outLen)` | copy out at most **one** complete frame; return `false` when none is ready |

```cpp
MyDriver       driver;
KnxCoordinator knx(&driver, PhysicalAddress{1, 1, 5});
KnxLight       lamp(knx, "0/1/1");
```

Everything above the driver is unchanged — device objects take a `KnxCoordinator&`, and
`Konnextra` *is* one.

`poll()` returning one frame per call is deliberate: the node loops until it returns `false`,
so the driver never has to think about how full the queue is. `sendTelegram()` returning the
real confirmation is the contract the rest of the library depends on — a driver that returns a
hard-coded `true` makes every send look successful and every failure invisible.

## Testing without hardware

Everything below `Konnextra` is free of Arduino headers, which is what lets the test suite run
on your computer:

```
pio test -e native
```

The tests replace the driver with a fake one that records what was sent and queues frames to
be received. That same trick works for your own sketch logic:

```cpp
class MockDriver : public IKnxDriver {
    public:
        bool sendResult = true;
        bool begin(void) override { return true; }
        bool reset(void) override { return true; }
        bool sendTelegram(const uint8_t* frame, uint8_t length) override {
            /* record it */ return sendResult;
        }
        bool poll(uint8_t* out, uint8_t maxLen, uint8_t& outLen) override {
            /* hand back a queued frame */ return false;
        }
};
```

Set `sendResult = false` to see how your sketch behaves when the bus does not confirm — a case
that is awkward to produce on real hardware and easy to get wrong.

## What it costs

- **No dynamic allocation.** There is no `new`, no `malloc` and no `std::vector` anywhere in
  the library. Device objects link themselves into a chain using a pointer they already carry,
  so adding one costs the object and nothing else.
- **Callbacks are plain function pointers**, not `std::function`, so an 8-bit board pays
  nothing for them. A non-capturing lambda converts to one; a capturing lambda will not
  compile.
- **Verbose tracing costs about 2.4 KB of flash and no RAM** when compiled in and switched off
  (measured on an ESP32-C6). A disabled log call is one boolean test — but its *arguments* are
  still evaluated, so guard an expensive one with `KnxDebug::isEnabled()`.
- **A send blocks for up to 100 ms** waiting for confirmation. Nothing else in the library
  blocks.
