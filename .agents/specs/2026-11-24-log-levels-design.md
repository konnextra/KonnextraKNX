# Design: log levels `Off / Basic / Deep` — approved direction, details at plan time

Date: 24.11.2026. Status: **direction agreed with Florian 24.11.2026**; implemented *after*
`subscribe` (`2026-11-23-subscribe-design.md`), because both share the telegram formatter.

## Why

Today there is one switch, `enableDebugMode(bool)`, and 57 `KnxDebug::log` call sites. Off
prints nothing at all — not even a failed start; on prints everything as loose lines that are
hard to read. Florian wants: silence by default, a readable xknx-style view for everyday use,
and a full trace on demand, with every telegram visually separated.

## Decided (Florian)

- `knx.setDebugMode(KnxLog::Off | KnxLog::Basic | KnxLog::Deep)`. **Default `Off`: nothing on
  Serial** — including start failures, reset and bus power loss (accepted consequence).
- Name `Basic` (Claude's pick, not objected to; "Short"/"Light" rejected as describing length).
- **Basic:** start/bring-up result or failure; one formatted line per incoming and outgoing
  telegram, roughly like xknx; on RX, whether listeners matched; problems (incomplete frame,
  checksum or decode error, send not confirmed, reset, power fail/return) flagged with a leading
  `!` and the hint `(more: setDebugMode(KnxLog::Deep))`.
- **Deep:** the Basic line plus, under it, how the telegram got there and everything it carries:
  raw bytes, control field, priority, repeat flag, hop count, length, checksum check, and per
  listener the decode result. Bring-up steps in full.
- **Every telegram in Deep is a visually separate block**, clearly set apart from other
  telegrams and from the start-up block.

## Proposed format (Claude, to confirm on a real log at plan time)

```
[knx] Basic:
[knx] start ok: NCN5130, 1.1.5
[knx] RX  1.1.1 -> 0/1/1  Write     01  (DPT1: on)  2 listeners
[knx] TX  1.1.5 -> 0/1/1  Write     00  confirmed
[knx] !   TX 1.1.5 -> 0/1/1  Write  00  not confirmed  (more: setDebugMode(KnxLog::Deep))

[knx] Deep:
[knx] ===== START ==============================================
[knx]   reset -> ok (12 ms)
[knx]   system status 0xFB: all ok, normal mode
[knx]   address 1.1.5 confirmed, auto-ack on
[knx]   start ok: NCN5130, 1.1.5
[knx] ----- RX 1.1.1 -> 0/1/1  Write  01  (DPT1: on) -----------
[knx]   raw    BC 11 01 00 01 E1 00 81 33
[knx]   ctrl   BC  priority low  repeat no
[knx]   route  hop count 6  length 1   checksum 33 ok
[knx]   listeners  subscribe 0/1/1 DPT1 -> on; KnxLight -> on
```
The decoded value in parentheses appears only when a listener supplies the DPT.

## Constraints / open for the plan

- `KnxTelegram` becomes Arduino `Printable` (`Serial.println(t)` gives the Basic line) — the
  one formatter shared by Basic, Deep and user code. Host build: a plain `format(char*, len)`.
- `enableDebugMode(bool)` stays as a shorthand (`true` → `Deep`) so 0.1.9 sketches compile.
- Output target: `knx.setDebugOutputPort(Print* port)` (Florian 24.11.: named like the debug
  switch, takes `&Serial2`, a BluetoothSerial, anything stream-like). Parameter type `Print*`
  rather than `Stream*`: every `Stream` is a `Print`, and output needs nothing more. Default
  `&Serial`; `nullptr` restores the default. Arduino-only (`#ifdef ARDUINO`); library-wide like
  the level itself.
- Read through the current 57 call sites against a real bench log before fixing the format.

## Parked: real bus monitor (`U_Busmon`)

Not part of this. Florian 24.11.: "lass den Busmonitor erstmal weg, merke ihn dir". Recorded in
`STATUS.md` → "Idee, noch nicht dran".
