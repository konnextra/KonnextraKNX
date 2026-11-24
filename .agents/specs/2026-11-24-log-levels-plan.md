# Log levels Implementation Plan

**Goal:** `knx.setDebugMode(KnxLog::Off|Basic|Deep)` + `knx.setDebugOutputPort(Print*)`, per
`2026-11-24-log-levels-design.md`. Executed inline (Florian 24.11.: "kannst auch gleich bauen").

**Architecture:** `KnxDebug` gets a level instead of a bool and four line kinds; a new
Arduino-free `KnxFormat` module renders values, telegram lines and frame details into char
buffers (host-tested); the coordinator and driver choose the kind per message. No `%f`
anywhere (AVR printf has no float) — floats are formatted by hand.

## Global constraints
- Default `Off`: not one byte on any port. `enableDebugMode(bool)` stays: `true` → `Deep`.
- Arduino-free core; `Print*` output only under `#ifdef ARDUINO`. Host build: output compiles away.
- No heap, stack buffers only (`LOG_BUFFER` 96 → 112 for the wider lines).
- Doxygen 0 warnings; `KnxLog` is user-facing (documented in `KnxEnums.h`, not excluded).

## Line kinds (KnxDebug)
| Function | Printed at | Shape |
|---|---|---|
| `info(fmt, …)` | Basic, Deep | `[knx] <text>` |
| `problem(fmt, …)` | Basic, Deep | `[knx] ! <text>`; at Basic followed by `  (more: setDebugMode(KnxLog::Deep))` |
| `section(fmt, …)` | Deep | `[knx] ----- <text> ----…` padded to 64 columns (`=====` for START) |
| `log(fmt, …)` / `logBytes` | Deep | `[knx]   <text>` (indented detail, existing call sites keep working) |

`isEnabled()` stays (true at Basic and Deep) for guards; new `level()` / `setLevel()` /
`isDeep()`; `setOutput(Print*)` Arduino-only, `nullptr` → `&Serial`.

## Messages per level
- **Start** (coordinator `begin()`/`reset()`): Deep section `===== START 1.1.5 =====`, driver
  steps as details. Driver result → `info("start ok: NCN5130, 1.1.5")` or
  `problem("start failed at <step>: <hint>")`.
- **TX** (`send`, `sendIndividual`, `sendControl`): Deep section `TX 1.1.5 -> 0/1/1  Write  (DPT1: off)`
  before building; build/driver/con as details. After the result, Basic line
  `TX  1.1.5 -> 0/1/1  Write     00  (DPT1: off)  confirmed` via `info`, or `problem(... not confirmed)`.
  At Deep the Basic line is not repeated; the section closes with detail `result: confirmed`.
- **RX** (`loop`): parse failure → `problem("RX frame dropped: <reason>")`. Otherwise Deep section
  with the telegram line, then details `raw`, `ctrl/prio/repeat`, `hops/length/checksum`, one
  detail per listener that decoded, `-> N listener(s)`. Basic: one `info` line
  `RX  1.1.1 -> 0/1/1  Write     01  (DPT1: on)  2 listeners` (or `no listener`).
  Decoded value in parentheses comes from the first subscription or object whose DPT decodes it
  (new `IKnxReceiver::receiverDpt()`, default `UNKNOWN`, `KnxObject` returns its DPT).
- **Chip events** (driver): reset pin low, unexpected Reset.ind, save pin low → `problem`;
  power back / re-running bring-up → `info`.
- **Address/decode warnings** stay `problem` (they are real mistakes in the sketch).

## KnxFormat (new, `src/KnxFormat.{h,cpp}`, internal, not in Doxyfile)
- `uint8_t formatValue(const KnxValue& v, char* out, uint8_t max)` — `DPT1: on`, `DPT5: 128`,
  `DPT9: 21.50`, `DPT10: 13:05:09`, `DPT11: 2026-11-24`, `DPT232: #FF8000`, …
- `uint8_t formatTelegram(const ParsedTelegram& t, char* out, uint8_t max)` — `1.1.1 -> 0/1/1  Write     01`
  (individual: `1.1.9 -> 1.1.5  T_Connect` / `APCI 0x300`).
- `uint8_t formatFrameDetails(const uint8_t* raw, uint8_t len, uint8_t line, char* out, uint8_t max)` — Deep detail lines.
- Tests: new `test/test_format/` with known-answer strings for each DPT kind, negative floats,
  Read/Response/Write lines, individual lines, buffer truncation (never writes past `max`, always NUL-terminated).

## Steps
1. KnxFormat + tests (RED → GREEN). Commit.
2. KnxDebug levels/kinds + `KnxLog` enum + `setDebugMode`/`setDebugOutputPort` on `KnxCoordinator`
   (and `KonnextraKNX` forwards automatically by inheritance). Host test for level gating
   (`isEnabled`/`isDeep` per level). Commit.
3. Reclassify call sites (coordinator, driver, frame, codec, address, object) per "Messages per level". Commit.
4. BenchTest + examples use `setDebugMode`; Changes.md; keywords; Doxygen; builds C6/Mega/R4; review. Commit.
