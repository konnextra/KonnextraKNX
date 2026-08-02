# How It Works {#howitworks}

You do not need this page to use the library. Read it when you want to know what happens
between `lamp.on()` and the light coming on, or when you want to add a device class the library
does not have yet.

## The layers

The library is a stack of small pieces, each of which only knows about the one below it.
Nothing ever reaches upwards.

| Layer | Does | You see it |
|---|---|---|
| `Konnextra` | the `knx` object you write in a sketch, and the driver it owns | always |
| Device objects | `KnxLight`, `KnxTemperature`, `KnxObject`, … a group address plus a datapoint type plus a cached value | always |
| `KnxCoordinator` | sends to group addresses, receives telegrams and hands them to the objects that want them | rarely |
| `KnxDriver` | speaks the transceiver's serial command set and reports whether a send was confirmed | no |
| Framing and codec | builds and parses telegrams, converts values to and from bus bytes | no |

`Konnextra` is the convenience on top. It is a `KnxCoordinator` that also constructs its own
`KnxDriver`, so you type the physical address once and nothing has to be wired together by
hand.

## What happens when you send

```cpp
lamp.on();
```

1. `KnxLight::on()` calls `write(Dpt1(true))`. The device class's only job is knowing that a
   light is a DPT 1.
2. `KnxObject::write()` updates its own cache immediately, so `isOn()` is right straight away,
   and passes the value on.
3. A telegram is built: the sender is this device's physical address, the destination is the
   command group address, and the value is encoded according to its datapoint type.
4. The driver sends the frame to the transceiver and waits for the bus to confirm it, up to
   100 ms.
5. That confirmation is the `true` or `false` you get back. It is never invented.

If a status telegram arrives later on the status address, it overwrites the assumed value with
what the bus actually reports.

## What happens when you receive

`knx.loop()` drains everything the driver has ready. It does not stop after one telegram.

1. The driver reassembles bytes from the transceiver into complete frames.
2. Each frame is parsed into sender, destination and payload. A bad checksum stops here.
3. If the telegram is addressed to a group, the library walks its list of objects and offers
   the address to each. Every object that recognises it gets the telegram, so two objects on
   one address both fire.
4. Each recipient decodes the payload with *its own* datapoint type. This is why a type
   mismatch produces a wrong value rather than an error. The bytes decode fine, just as
   something else.
5. The cached value is updated and your callback runs, inside `loop()`, on your own task. There
   is no interrupt and no separate thread anywhere in the library.

Telegrams addressed to this device individually take a different path and are ignored unless
you install a handler for them. Telegrams for other devices are seen and dropped. With tracing
on they are logged as reaching "0 receiver(s)", which is normal.

## Writing your own device class

A device class is a thin layer over `KnxObject` that hides the datapoint type behind names that
mean something. `KnxLight` is barely sixty lines, and most of them are constructors.

The pattern: pick the type in the constructor, wrap `write()` in named methods, and override
`onValueUpdated()` to turn the cached value into a callback with a real type.

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

- `cachedValue` is `protected`, so your class reads it directly. It has already been decoded
  and checked by the time `onValueUpdated()` runs.
- Overriding `onValueUpdated()` *replaces* the generic callback rather than adding to it. The
  built-in classes do the same. Call `KnxObject::onValueUpdated()` from your override if you
  want both.
- Registration is automatic. The constructor attaches the object and the destructor detaches
  it, so an object that goes out of scope simply stops receiving.

Note the constructor takes a `KnxCoordinator&`, not a `Konnextra&`. That costs you nothing:
`Konnextra` is a `KnxCoordinator`, so passing your `knx` object works as it does for every
built-in class.

## Testing without hardware

Everything below `Konnextra` is free of Arduino headers, which is what lets the test suite run
on your computer instead of on a board:

```bash
pio test -e native
```

**This is a PlatformIO feature.** The Arduino IDE has no equivalent, so if that is your
toolchain, this section is not available to you. Everything else in the library works the same
either way.

The tests replace the driver with a fake one that records what was sent and queues frames to be
received. The same trick works for your own sketch logic:

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

MockDriver     driver;
KnxCoordinator knx(&driver, PhysicalAddress{1, 1, 5});
```

Set `sendResult = false` to see how your sketch behaves when the bus does not confirm a send.
That case is awkward to produce on real hardware and easy to get wrong.

## What it costs

- **No dynamic allocation.** There is no `new`, no `malloc` and no `std::vector` anywhere in
  the library. Device objects link themselves into a chain using a pointer they already carry,
  so adding one costs the object and nothing else.
- **Callbacks are plain function pointers**, not `std::function`, so an 8-bit board pays nothing
  for them. A non-capturing lambda converts to one. A capturing lambda will not compile.
- **Verbose tracing costs about 2.4 KB of flash and no RAM** when compiled in and switched off,
  measured on an ESP32-C6. A disabled log call is one boolean test. Its *arguments* are still
  evaluated though, so guard an expensive one with `KnxDebug::isEnabled()`.
- **A send blocks for up to 100 ms** waiting for confirmation. Nothing else in the library
  blocks.
