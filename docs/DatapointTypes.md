# Datapoint Types {#datapoints}

Every KNX telegram carries a value, and every value has a **datapoint type** that says how
its bytes are to be read. A telegram alone does not tell you whether `0xFF` means "on",
"255" or "100 %" — the datapoint type does.

Both ends of a group address must agree on the type. If they disagree, telegrams still
arrive and still pass their checksum; they simply decode to the wrong number, or fail to
decode at all. That mismatch is the single most common cause of values that look wrong —
see @ref troubleshooting.

You rarely name a type directly. A device object already knows the right one: a KnxLight is
DPT 1 because a light is on or off. You name one when you use ::KnxObject for a datapoint
that has no dedicated class.

## The types

| Type | Carries | Create with | Read with | Used by |
|---|---|---|---|---|
| **DPT 1** | on/off, true/false | `Dpt1(bool)` | `asBool()` | KnxLight, KnxDimmLight, KnxBlind |
| **DPT 2** | a value 0…3 | `Dpt2(uint8_t)` | `asU8()` | — |
| **DPT 3** | a relative dim or blind step | `Dpt3(bool increase, uint8_t stepcode)` | `asDim()` | KnxDimmLight |
| **DPT 4** | a single character | `Dpt4(char)` | `asChar()` | KnxChar |
| **DPT 5** | unsigned 0…255, also 0…100 % | `Dpt5(uint8_t)` | `asU8()` | KnxPercent |
| **DPT 6** | signed −128…127 | `Dpt6(int8_t)` | `asI8()` | — |
| **DPT 7** | unsigned 0…65535 | `Dpt7(uint16_t)` | `asU16()` | — |
| **DPT 8** | signed −32768…32767 | `Dpt8(int16_t)` | `asI16()` | — |
| **DPT 9** | 16-bit float — temperatures, humidity | `Dpt9(float)` | `asFloat()` | KnxTemperature, KnxHumidity |
| **DPT 10** | time of day | `Dpt10(DptTime)` | `asTime()` | KnxTime |
| **DPT 11** | calendar date | `Dpt11(DptDate)` | `asDate()` | KnxDate |
| **DPT 12** | unsigned 32-bit | `Dpt12(uint32_t)` | `asU32()` | — |
| **DPT 13** | signed 32-bit | `Dpt13(int32_t)` | `asI32()` | — |
| **DPT 14** | 32-bit float | `Dpt14(float)` | `asFloat()` | KnxFloat |
| **DPT 19** | date and time together | `Dpt19(DptDateTime)` | `asDateTime()` | KnxDateTime |
| **DPT 232** | RGB colour | `Dpt232(r, g, b)` | `asColor()` | KnxRGB |

Types with no dedicated class in the last column are still fully usable — reach them through
::KnxObject:

```cpp
KnxObject counter(knx, "0/5/0", KnxDpt::DPT7);
counter.write(Dpt7(1000));
```

Each factory accepts only what its datapoint can carry, so passing the wrong kind of value
is a compile error rather than a bad telegram on the bus.

## Composite values

Five types carry more than a single number. They are small plain structs — build one, fill
its fields, hand it to the factory.

**::DptDim** — a relative dimming or blind step.

| Field | Meaning |
|---|---|
| `increase` | `true` = brighter / up, `false` = darker / down |
| `stepcode` | `0` = stop, `1`…`7` = step size |

**::DptTime** — a time of day.

| Field | Range |
|---|---|
| `weekday` | 0 = none, 1 = Monday … 7 = Sunday |
| `hour` | 0…23 |
| `minute` | 0…59 |
| `second` | 0…59 |

**::DptDate** — a calendar date: `day` 1…31, `month` 1…12, `year` as the full year, e.g. 2026.

**::DptDateTime** — date and time in one telegram: `year`, `month`, `day`, `weekday`, `hour`,
`minute`, `second`, plus `summerTime` (daylight saving active) and `faultFlag` (the sending
clock reports its data as unreliable). Check `faultFlag` before trusting the rest.

**::DptColor** — an RGB colour: `r`, `g`, `b`, each 0…255.

## Two things worth knowing

**DPT 5 is raw on the wire, percent in KnxPercent.** The bus carries 0…255. `KnxPercent`
converts in both directions — you write and read 0…100, and it rounds to the nearest raw
step. Reading the same address with a plain `KnxObject` of type DPT 5 gives you the raw
0…255 instead. Both are correct; they are just different scales, and mixing them up is why a
"100 %" value sometimes reads as 255.

**DPT 9 and DPT 14 are both floats, and not interchangeable.** DPT 9 is a compact 16-bit
format used for temperatures and humidity — limited precision, and that is what actuators
and sensors normally speak. DPT 14 is a full 32-bit float. Sending DPT 14 where the other
device expects DPT 9 produces a decode failure, not a rounded value.
