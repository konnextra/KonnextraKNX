# KNX Basics {#knxbasics}

You do not need to know the KNX standard to use this library, but four ideas come up in every
sketch: the two kinds of address, how devices find each other without a central controller,
what a datapoint type is, and what it means that there is no ETS project. This page covers them
in the order you meet them.

## Two kinds of address

KNX uses two address formats, and they mean different things. Mixing them up is the first
mistake everyone makes.

| | Looks like | Answers | You write it |
|---|---|---|---|
| **Physical address** | `1.1.5` | *which device is this?* | once, when you create `knx` |
| **Group address** | `0/1/1` | *what is being talked about?* | on every device object |

A physical address belongs to one piece of hardware and stays with it for its whole life. A
group address names a *topic*, for example "the kitchen ceiling light" or "the outside
temperature", and it is what devices actually use to talk to each other.

```cpp
Konnextra knx("1.1.5");                 // who this device is
KnxLight  lamp(knx, "0/1/1", "0/3/0");  // what it talks about
```

## Group addresses are the wiring

There is no central controller on a KNX bus. A switch does not know which actuator it operates.
It writes a value to a group address, and every device listening on that address reacts. One
sender, any number of listeners, nothing set up beforehand.

That is why replacing a lamp actuator changes nothing in your sketch. The new one is configured
to listen on the same group address, and the conversation carries on.

A group address has three parts, written `main/middle/sub`:

| Part | Range |
|---|---|
| main | 0…31 |
| middle | 0…7 |
| sub | 0…255 |

The split has no technical meaning. It is a naming scheme, and installations use it to group by
function and floor. A common convention is main = function, middle = room or floor, sub = the
individual device. Follow whatever the existing installation already does. `0/0/0` is reserved
and is not a usable address.

Most device objects take a **command** address and a **status** address, because a KNX actuator
normally reports back on a different address than it is commanded on. @ref examples shows both.

## Physical addresses are for identity, not traffic

Your device's physical address goes on every telegram it sends, as the sender. Beyond that it
is only used for device management, the point-to-point messages an ETS installation uses to
identify, ping and program a device.

The library receives those telegrams and ignores them unless you install a handler. It never
programs itself from the bus, so nothing an ETS session does can change what your sketch is
doing. `KnxCoordinator::setDeviceHandler()` and `KnxCoordinator::sendIndividual()` are there if
you want to take part in that traffic. You can ignore both.

Physical addresses read `area.line.device`, with area and line 1…15 and device 1…255. Pick one
that is free on your installation. Two devices sharing an address is the same problem as two
computers sharing an IP.

## Bad addresses are corrected, not rejected

Addresses are parsed from strings, and a string can say anything. When a part is out of range
the library clamps it to the nearest legal value and carries on. Nothing throws, and the device
still comes up:

| Written | Becomes | Logged |
|---|---|---|
| `"1.1.300"` | `1.1.255` | `ADR !! device out of range (1-255), clamped to 255` |
| `"20.1.5"` | `15.1.5` | `ADR !! area out of range (1-15), clamped to 15` |
| `"0/9/1"` | `0/7/1` | `ADR !! middle group out of range (0-7), clamped to 7` |
| `"40/1/1"` | `31/1/1` | `ADR !! main group out of range (0-31), clamped to 31` |
| `"0/0/0"` | `0/0/1` | `ADR !! 0/0/0 is not a valid group address, using 0/0/1` |

A malformed string, a missing separator or a typo, is not reported as such. Its parts read as
`0` or as something meaningless, and you see the clamp warnings instead, or no warning at all.
An address the library accepts is therefore not necessarily the address you meant.

Those lines only appear with tracing on:

```cpp
knx.enableDebugMode(true);
```

If a device object never reacts and never sends, read its address back from the log before
suspecting the wiring.

## Datapoint types

A telegram carries raw bytes. The **datapoint type** says how to read them. The same byte can
be an on/off state, a number from 0 to 255, or a percentage, depending on which type the two
ends agreed on. And they must agree: a mismatch decodes to a wrong value rather than to an
error.

Device objects already know their type. A `KnxLight` is DPT 1 because a light is on or off. You
only name a type yourself when you use `KnxObject` for a datapoint that has no dedicated class.
@ref datapoints lists every type, how to build a value and how to read one.

## There is no ETS project

ETS is the commercial tool that configures a KNX installation. It assigns physical addresses,
links group addresses to device functions, and downloads that configuration into each device.

This library does none of that. Group addresses live in your sketch, as strings you type. That
is a deliberate trade, the same one an Arduino sketch always makes over a configuration tool.
You give up central management and gain a device you can flash and change in seconds, with no
licence and no project file.

### Finding out which addresses to use

The catch is real and worth stating plainly: **you have to know your group addresses**, and
nothing in the library can tell you what they are. Three ways to find out, best first.

**Read them off the ETS project.** If the installation was set up with ETS, someone has the
project file or a printed group address list. That is the reliable answer, and it also tells you
each address's datapoint type, which you need anyway.

**Ask the installer.** Most hand over a group address list on request.

**Watch the bus.** If neither is available, you can discover addresses by listening. Turn
tracing on and run a sketch that does nothing but receive:

```cpp
#include <Konnextra.h>

Konnextra knx("1.1.5");     // pick a free address

void setup() {
    Serial.begin(115200);
    knx.enableDebugMode(true);
    knx.begin();
}

void loop() {
    knx.loop();
}
```

Every telegram on the bus is logged, including those meant for other devices:

```
[knx] RX  <- 1.1.2 to GA 0/1/1 -> 0 receiver(s)
```

Press a wall switch and see which address appears. Move a blind and note the address it uses.
"0 receiver(s)" is normal here, it just means no object in *your* sketch listens on it.

Two limits to this method. It only shows addresses that something actually sends to, so a
status address nobody writes to stays invisible. And it does not tell you the datapoint type,
which you still have to infer from what the device does, or confirm by trying it.

Your sketch is not visible in the ETS project either. Nobody can accidentally reprogram it, and
nobody can see what it does.

## Next

- @ref datapoints for the value types and how to build them.
- @ref examples for complete sketches.
- @ref boards for what to run this on.
