# Troubleshooting {#troubleshooting}

Work through this page in order. Most problems are one of four things: the sketch never
reaches the transceiver, the transceiver never reaches the bus, the bus never answers, or
the answer arrives but is not understood.

## First: turn on tracing

Almost every question below is answered by one line of the log.

```cpp
knx.enableDebugMode(true);   // before begin(), so start-up is traced too
```

Everything it prints is prefixed `[knx]`. Leave it off in normal operation — the printing
itself takes time on the receive path, so it can disturb what you are trying to observe.
Get a clean run first, then switch it on to explain a broken one.

A healthy start-up looks roughly like this:

```
[knx] DRV port up @ 19200 baud 8E1
[knx] DRV begin: reset ok, state 0x07
```

If you never see the first line, the problem is in your sketch, not on the bus.

---

## Nothing compiles

**`use of deleted function 'Konnextra::Konnextra(const String&)'`**

Your board has no hardware serial port free for KNX — its only one is the USB console. The
Uno is the usual case. Name the port explicitly and accept that the serial monitor is gone:

```cpp
Konnextra knx("1.1.5", Serial);
```

**`undefined reference to 'Serial1'`**

The port exists as a name but your core never created it. On STM32 boards this is normal:
add `-DENABLE_HWSERIAL1` to your build flags, or name a different port.

---

## `begin()` returns false

`begin()` is `true` only when the transceiver answered the reset handshake *and* reported a
healthy state. A `false` means it did not answer at all.

| Check | What to look for |
|---|---|
| **Port** | Is the transceiver really on the port you named? On a board with several, `Serial1` is a guess that is often wrong. |
| **Wiring** | RX and TX crossed is the single most common cause. Your board's RX must meet the transceiver's TX. |
| **Pins (ESP32 only)** | The UART starts on the core's default pins, which are almost certainly not yours. Call `setPins(rx, tx)` before `begin()`. |
| **Line settings** | 19200 8E1. If you opened the port yourself and passed it as a stream, nothing enforces this — 8N1 looks identical in code and fails on the wire. |
| **Power** | The transceiver needs the bus to be live, not just the microcontroller. |

There is no second attempt: the transceiver has no reset line, so an unanswered reset is
reported as a failure rather than retried.

---

## It sends, but nothing happens

**The log shows a frame going out and then `no con within 100 ms`.**

The bytes reached the transceiver but it never confirmed them. Look past the library: bus
power, the transceiver itself, or a frame the bus genuinely refused.

**The log shows `con byte 0x0B -> NEGATIVE`.**

The bus saw the telegram and rejected it. Usually nobody is listening on that group
address, or another device is holding the line.

**Everything is confirmed but the actuator does nothing.**

The telegram is fine and the bus took it — so the problem is on the other end. The actuator
is not configured to act on that group address. Confirm the address in ETS or on the device
itself; this library cannot tell you what the other device expects.

---

## It never receives anything

**No `[knx]` receive lines at all.**

Either nothing is being sent to you, or `loop()` is not running often enough.

```cpp
void loop() {
    knx.loop();     // must run constantly
    delay(5000);    // <- this is the bug
}
```

`delay()` stops everything, including reception. Telegrams that arrive during it are lost.
Use `millis()` for timing instead — every example in this documentation does.

**Receive lines appear, but end in `0 receiver(s)`.**

```
[knx] RX  <- 1.1.7 to GA 0/4/2 -> 0 receiver(s)
```

This is normal. You are seeing traffic addressed to other devices on the bus. It only
matters if telegrams you *expect* are logged this way — then the group address in your
object does not match the one on the wire. The address in the log is the truth.

**Something arrives, but your callback never runs.**

Check which address your object listens on. Most device objects take a *command* address
and a *status* address, and status is the one that fires callbacks:

```cpp
KnxLight lamp(knx, "0/1/1", "0/3/0");   // sends to 0/1/1, listens on 0/3/0
```

Sending and listening on the same address is a common mistake — many actuators publish
their status on a different address than the one they take commands on.

---

## It receives, but the values are wrong

**Values are always 0, or always maximum.**

The datapoint type does not match what the sender uses. A brightness sent as a percentage
and read as a raw byte gives exactly this. The Datapoint Types page lists what each type
carries.

**The log says the payload could not be decoded.**

```
[knx] OBJ !! matched GA but decode failed (object DPT 5)
```

The telegram reached the right object — the group address matched — but its payload does
not fit the datapoint type you declared. The sender uses a different type than you assumed.
The number in the message is the type *your* object expects.

**Values jump or are occasionally nonsense.**

Suspect the line settings before the library. A parity mismatch corrupts individual bytes
while most frames still pass their checksum.

---

## It works, then stops

**After adding WiFi, MQTT or a display.**

Those stacks can starve `loop()`. The bit-level bus timing runs on the transceiver's own
co-processor and is safe, but reception still depends on your sketch calling `knx.loop()`
often. Anything that blocks for tens of milliseconds will cost you telegrams.

**Only under load, or only sometimes.**

Turn tracing on and watch whether frames arrive and fail to parse, or never arrive. Those
are different problems: the first is line quality or timing, the second is your loop.

---

## Reading the log

| Line | Meaning |
|---|---|
| `DRV port up @ 19200 baud 8E1` | the port was opened by the library |
| `DRV using caller-configured stream` | you opened it — the settings are yours to get right |
| `DRV begin: reset ok, state 0x07` | the transceiver is alive and healthy |
| `DRV tx frame: …` | bytes handed to the transceiver |
| `DRV echo n/n ok` | the transceiver echoed the frame back intact |
| `DRV con byte 0x8B -> positive` | the bus confirmed the send |
| `DRV !! no con within 100 ms` | no confirmation arrived |
| `RX  frame: …` | raw bytes reassembled from the bus |
| `RX  !! parse failed` | bytes arrived but did not form a valid telegram |
| `RX  <- … -> 0 receiver(s)` | a telegram for some other device |
| `OBJ !! matched GA but decode failed` | right object, wrong datapoint type |

## Still stuck

Open an issue with the tracing output of a full start-up and one failing cycle, the board
you are on, and how the transceiver is wired. The log is far more useful than a description
of the symptom.
