# FAQ {#faq}

Questions about what the library can do and why it works the way it does. If something is
broken rather than unclear, @ref troubleshooting is the page you want.

## Getting started

**Do I need ETS?**
No. Group addresses are string literals in your sketch. @ref knxbasics explains what that
costs you and what it does not.

**Do I need the %Konnextra Bridge, or does any TP-UART work?**
Any TP-UART-compatible transceiver works. The library speaks the TP-UART2 command set over a
serial port at 19200 8E1 and does not care what implements it. The Bridge presents that
interface from an STKNX front end, a plain TP-UART2 module presents it directly, and the
library has been used against both.

**Which boards does it run on?**
In principle every board the Arduino framework supports. @ref boards lists the ones that are
built on every commit and explains the one board that genuinely cannot work, the Uno, which has
no spare serial port.

**Which serial port does it use, and can I change it?**
`Serial1` by default, and yes. That whole question lives on @ref boards, including what to do
when your board has no spare port.

## What it can and cannot do

**Can I ask the bus for the current state at start-up?**
No. The library only ever *writes* values. There is no way to send a read request, and until a
status telegram arrives an object's `value()` returns its default (`false`, `0`). Two practical
consequences: a freshly booted sketch does not know whether the lamp is on, and if your
installation only publishes on change, that can last a long time. If you need state at boot,
have the actuator send cyclically, or design the sketch not to depend on it.

**Does it answer read requests from other devices?**
No. A read request addressed to a group address your objects listen on is currently decoded as
a value of zero, which overwrites the cached value and fires the callback. That is a known
defect, not intended behaviour. It only bites you if something on your installation polls, which
ETS and some visualizations do.

**Can several objects use the same group address?**
Yes. Every telegram is offered to every registered object, and each one that recognises the
address gets it. Two objects listening on one status address both fire, each with its own
callback. Nothing has to be unique.

**How many device objects can I have?**
There is no fixed limit. Objects link themselves into a chain when they are created, so the cost
is memory, not a preallocated table. In practice you run out of RAM long after you run out of
patience typing group addresses.

**What about datapoint types that have no device class?**
Use `KnxObject` and name the type yourself. @ref datapoints lists every supported type and
which ones already have a class.

**Does it support KNX IP or KNX RF?**
No. This is twisted-pair KNX only. The library talks to a TP transceiver over a serial line,
with no IP tunnelling and no radio.

## Living with it

**How often does `loop()` really need to run?**
Every pass, and without long blocking calls in between. Incoming telegrams sit in the driver
until `loop()` collects them, so a `delay(5000)` is five seconds of not receiving. Use
`millis()` for timing. This is the single most common cause of "callbacks sometimes do not
fire".

**Does sending block?**
Briefly. A send waits for the bus to confirm it, up to 100 ms, and returns `true` only if the
confirmation arrived. So a send is not free, but it also never leaves you guessing:

```cpp
if (!lamp.on()) {
    // the bus did not confirm it
}
```

@ref troubleshooting covers what an unconfirmed send usually means.

**Does WiFi, MQTT or Matter disturb the bus?**
By design, no. Everything timing-critical happens on the transceiver, not on your MCU. The
library never installs an interrupt handler and never busy-waits on bit timing, so another stack
stealing CPU time delays your telegrams rather than corrupting them. What it will do is delay
`loop()`, which is the paragraph above.

**Is it safe to use from more than one task?**
No. There is no locking anywhere in the library. Keep `knx` and its objects on a single task,
and pass values across to other tasks yourself.

**What does it cost in flash and RAM?**
Small enough that it has never been the constraint, but no number is quoted here because it has
not been measured across boards. The one figure that has been: verbose tracing costs about
2.4 KB of flash and no RAM when compiled in and switched off, measured on an ESP32-C6. A
disabled log call is one boolean test.

## Design decisions

**Why are group addresses strings and not numbers?**
Because that is how they are written down everywhere else: in the ETS project, on the label, in
the email from the electrician. They are parsed once at construction into a packed 16-bit value,
so nothing is re-parsed while the bus is running. Out-of-range parts are clamped rather than
rejected, and @ref knxbasics has the table.

**Why must device objects be global?**
They register themselves when they are created and unregister when destroyed, so an object that
goes out of scope stops receiving. Declaring them at file scope, which is the usual Arduino
style anyway, is the simple way to guarantee they live long enough.

**Why does the callback take a plain function pointer and not a `std::function`?**
To keep the library usable on 8-bit boards, where `std::function` means dynamic allocation. A
non-capturing lambda converts to a function pointer and works exactly the same way. A capturing
one will not compile, which is intentional.

**Why is `enableDebugMode()` library-wide rather than per object?**
It is a log level, not a service. Making it global is what lets the framing and codec layers
trace without owning an object, and a sketch with two bus connections almost certainly wants to
see both.
