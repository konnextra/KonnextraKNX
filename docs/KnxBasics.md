# KNX Basics {#knxbasics}

You do not need to know the KNX standard to use this library, but four ideas come up in
every sketch: the two kinds of address, what a datapoint type is, how devices find each
other without a central controller, and why there is no ETS project here. This page covers
them in the order you meet them.

## Two kinds of address

KNX uses two address formats, and they mean completely different things. Mixing them up is
the first mistake everyone makes.

| | Looks like | Answers | You write it |
|---|---|---|---|
| **Physical address** | `1.1.5` | *which device is this?* | once, when you create the node |
| **Group address** | `0/1/1` | *what is being talked about?* | on every device object |

A physical address identifies one piece of hardware, the way a house number identifies one
house. A group address identifies a *topic* — "the kitchen ceiling light", "the outside
temperature" — and it is what devices actually use to talk to each other.

```cpp
Konnextra knx("1.1.5");                 // who this device is
KnxLight  lamp(knx, "0/1/1", "0/3/0");  // what it talks about
```

## Group addresses are the wiring

There is no central controller on a KNX bus. A switch does not know which actuator it
operates — it writes a value to a group address, and **every device listening on that
address reacts**. One sender, any number of listeners, no connection set up beforehand.

That is why replacing a lamp actuator changes nothing in your sketch: the new one is
configured to listen on the same group address, and the conversation carries on.

A group address has three parts, written `main/middle/sub`:

| Part | Range |
|---|---|
| main | 0…31 |
| middle | 0…7 |
| sub | 0…255 |

The split has no technical meaning — it is a naming scheme, and installations use it to
group by function and floor (a common convention is main = function, middle = room or
floor, sub = the individual device). Follow whatever the existing installation already
does. `0/0/0` is reserved and is not a usable address.

Most device objects take a **command** address and a **status** address, because a KNX
actuator normally reports back on a different address than it is commanded on. Getting
Started covers that split and the one-address shorthand.

## Physical addresses are for identity, not traffic

Your node's physical address goes on every telegram it sends, as the sender. Beyond that it
is only used for device management — the point-to-point messages an ETS installation uses
to identify, ping and program a device.

The library receives those telegrams and, unless you install a handler for them, **ignores
them**. It never programs itself from the bus, so nothing an ETS session does can change
what your sketch is doing. `KnxCoordinator::setDeviceHandler()` and
`KnxCoordinator::sendIndividual()` are there if you want to take part in that traffic; you
can ignore both.

Physical addresses read `area.line.device`, with area and line 1…15 and device 1…255. Pick
one that is free on your installation — two devices sharing an address is the same problem
as two hosts sharing an IP.

## Bad addresses are corrected, not rejected

Addresses are parsed from strings, and a string can say anything. When a part is out of
range the library **clamps it to the nearest legal value and carries on** — nothing throws,
and the node still comes up:

| Written | Becomes | Logged |
|---|---|---|
| `"1.1.300"` | `1.1.255` | `ADR !! device out of range (1-255), clamped to 255` |
| `"20.1.5"` | `15.1.5` | `ADR !! area out of range (1-15), clamped to 15` |
| `"0/9/1"` | `0/7/1` | `ADR !! middle group out of range (0-7), clamped to 7` |
| `"40/1/1"` | `31/1/1` | `ADR !! main group out of range (0-31), clamped to 31` |
| `"0/0/0"` | `0/0/1` | `ADR !! 0/0/0 is not a valid group address, using 0/0/1` |

A malformed string — a missing separator, a typo — is not reported as such. Its parts read
as `0` or as something meaningless, and you see the clamp warnings instead, or no warning
at all. So an address the library silently accepts is not necessarily the address you
meant.

Those lines only appear with tracing on:

```cpp
knx.enableDebugMode(true);
```

If a device object never reacts and never sends, read its address back from the log before
suspecting the wiring.

## Datapoint types

A telegram carries raw bytes; the **datapoint type** says how to read them. `0xFF` is "on",
"255" or "100 %" depending on which type both ends agreed on — and they must agree, because
a mismatch decodes to a wrong value rather than to an error.

Device objects already know their type: a `KnxLight` is DPT 1 because a light is on or off.
You only name one when you use `KnxObject` for a datapoint that has no dedicated class.
@ref datapoints lists every type, its factory and its accessor.

## Why there is no ETS project

ETS is the commercial tool that configures a KNX installation: it assigns physical
addresses, links group addresses to device functions, and downloads that configuration into
each device.

This library does none of that. **Group addresses live in your sketch**, as string literals
you type. That is a deliberate trade — the same one an Arduino sketch always makes over a
configuration tool. You give up central management and gain a device you can flash and
change in seconds, with no licence and no project file.

It does not put you outside an existing installation. The addresses are the interface: read
the group addresses off the ETS project that is already running, write those same strings in
your sketch, and your node joins the conversation like any other device. What ETS will not
show you is *this* node — it is not in the project, so nobody can accidentally reprogram it,
and nobody can see what it does either.

---

Next: @ref boards for what to run this on, or @ref examples for complete sketches.
