# Design: stateless receive (`subscribe` + `onTelegram`) — DRAFT

Date: 23.11.2026. Status: **approved by Florian 24.11.2026** (name `subscribe`, table size raised).

## Why

`knx.send("0/1/1", Dpt1(true))` sends without an object; receiving still needs a global
`KnxObject`. LostWolf (knx-user-forum, #11) wants to listen without one, e.g. a display that
only shows status values. Florian promised the counterpart (#12) and sketched the Tasmota/Berry
side (#14: `knx.subscribe("0/1/1", callback, "DPT1.001")`). The Tasmota layer subscribes at
runtime from Berry and needs per-subscription context (the Berry closure).

## Goals / non-goals / anti-goals

- Goal: the direct counterpart to `send()` for sketches — one line in `setup()`, no global object.
- Goal: a raw hook an integration layer (Tasmota, ESPHome) can build its own subscription
  handling on, with no library-side limit.
- Non-goal: answering GroupValueRead (separate later feature), changing `KnxObject` behaviour,
  the Tasmota layer itself.
- Anti-goal: dynamic allocation; a subscribe callback firing on a read request with value 0
  (the known `KnxObject` defect, PLAN.md) — the new path must not inherit it.

## Approaches (Florian picked B + A)

- **A — fixed subscription table** in the coordinator: `knx.subscribe(ga, dpt, cb)`. Beginner
  friendly; costs RAM for unused slots; capped.
- **B — one catch-all hook** `knx.onTelegram(cb, ctx)`: every group telegram (Write, Response,
  Read, foreign traffic), user decodes. Zero storage, no cap, ideal for integrations.
- C — a slim `KnxListener` object: rejected, it is `KnxObject` without send and does not remove
  the object.

**Picked: B as the foundation, A as a thin convenience beside it.** A is *not* implemented by
occupying B's single slot — both live in the coordinator, so a sketch can use both at once.

## Prior art: the thesis `bind()` — and why this reverses PLAN §6/§7

The thesis stack (first commit `0738a5f`, `lib/KNX`) already had approach A:
`knx.bind("0/3/0", KNX_DPT::DPT1, cb)`, a fixed `KNX_Binding bindings[32]` of
`{GA, DPT, callback}`, `false` when full, every matching entry fired, decode failures skipped.
The callback received a `KnxEvent {source, target, type, dpt, value}`. There was no catch-all hook.

The redesign removed it on purpose (PLAN §6/§7, commit `73e1a66`): `bind()` "leaks the DPT
while sending hides it", and "the stateless tier has no receive by definition" — `KnxObject` +
`onUpdate()` was to cover it. That reasoning no longer holds: `send()` names the DPT too, inside
the value (`Dpt1(true)`), and on receive the DPT cannot be inferred from the bus, so naming it is
unavoidable, not a leak. The `KnxObject` replacement needs a global object, which is exactly what
LostWolf objects to and what Tasmota cannot create at runtime. Recorded as `decisions/0004`. (PLAN.md has since been
rewritten; §6/§7 exist only in git history, so there is no section left to annotate.)

Kept from `bind()`: the event-style callback (Florian's pick, 23.11.) — the callback sees
source, group address and type, not just the value.

## Design

One public event type, **`KnxTelegram`**, is handed to both callbacks: a small read-only view
over `ParsedTelegram` (the name is free; only a stale comment in `KnxDebug.h` mentions it).
- `groupAddress()` (packed), `source()`, `type()` (Read/Response/Write), `isRead()`.
- `value()` — the value decoded with the subscription's DPT; invalid in `onTelegram` (no DPT known).
- `decode(KnxDpt)` → `KnxValue`, invalid on a Read or a decode failure.
- `payload()` / `payloadLength()` raw, for DPTs the codec lacks.
- The inline-6 vs. payload selection now in `KnxObject::receive()` moves into
  `KnxCodec::decodeTelegram(dpt, parsed)`; `KnxObject` and `KnxTelegram::decode()` both call it.
  `KnxObject` keeps its current behaviour (it still decodes Reads — the known PLAN.md defect is
  not fixed here); only `KnxTelegram::decode()` adds the Read check.

Each callback has two accepted shapes, so beginners never see the context pointer:
`void cb(const KnxTelegram& t)` and `void cb(const KnxTelegram& t, void* ctx)`.

### B — `onTelegram`
```cpp
void onBusTelegram(const KnxTelegram& t) {
	if (t.type() == GroupValueType::Write && t.groupAddress() == packedGroupAddressFromString("0/3/0")) {
		bool on = t.decode(KnxDpt::DPT1).asBool();
	}
}
knx.onTelegram(onBusTelegram);
knx.onTelegram(onBusTelegramCtx, &myState);   // integration layers
```
- One slot (function pointer + `void*`); a second call replaces it, `nullptr` removes it.
- Fires for **every** group telegram with an APCI, before the receiver registry, including Read.

### A — `subscribe`
```cpp
void onStatus(const KnxTelegram& t) { bool on = t.value().asBool(); }
knx.subscribe("0/3/0", KnxDpt::DPT1, onStatus);
knx.subscribe("0/4/2", KnxDpt::DPT9, onTempCtx, &ctx);   // void onTempCtx(const KnxTelegram&, void*)
knx.unsubscribe("0/3/0");                                // removes every entry on that GA
```
- Returns `bool`: `false` when the table is full (logged in debug mode) or the callback is
  `nullptr`. A malformed GA string behaves exactly as in `send()` (logged, clamped).
- Fires on **Write and Response only**, after a successful decode; a decode failure is logged
  (debug) and skipped — same as `KnxObject`.
- Several entries may share a GA (each fires), in slot order. `unsubscribe` frees slots
  without compacting, so it is safe from inside a callback; `subscribe` takes the first free slot.
- Table size: `knx.setSubscriptionSlotLimit(uint16_t total)`, default **64** on every board
  (Florian, 24.11.). Sets the total, not an increment. The table is allocated **once**, with
  `malloc`, on the first `subscribe()`, and never freed or resized — the one sanctioned
  exception to "no dynamic allocation" (see decision 0004). `setSubscriptionSlotLimit()`
  returns `false` once the table exists, so in practice it belongs in `setup()`. A sketch that
  never subscribes uses 0 bytes. If `malloc` fails, `subscribe()` returns `false` (logged).
  One slot: 12 bytes on 32-bit (64 = 768 B), 8 on AVR (64 = 512 B of the Mega's 8 KB).
  The thesis `bind()` had a fixed 32.
- Both overloads exist for `uint16_t` GAs and Arduino `String` GAs, mirroring `send()`.

### Dispatch order in `KnxCoordinator::dispatch()`
1. `onTelegram` hook (if set). 2. subscription table. 3. receiver registry (unchanged).
The debug line "N receiver(s)" counts subscription hits plus registry hits.

## Constraints

Arduino-free core (host-testable), no `new`, C++11/AVR-safe (`<stdint.h>`, no address of
`static constexpr`), camelCase methods, `#define` for the size, zero Doxygen warnings, any new
public header added to the `Doxyfile` `INPUT`.

## Verification

Host tests in `test_coordinator` (mock driver): subscribe fires on Write and Response, not on
Read; full table returns `false`; shared GA fires both; unsubscribe; ctx passed through;
`onTelegram` sees Read and foreign GAs; replacing/removing the hook; subscribe + object + hook
on the same GA all fire. `test_object` stays green after the decode refactor. Portability
matrix builds. On hardware (later): subscribe to the button's 0/1/1 on the test case.

## Left to the builder / follow-ups

- Exact `KnxTelegram` file placement (likely `KnxTelegramTypes.h` or its own header).
- New example `examples/Subscribe/` + `Changes.md` entry; the website Examples guide (en+de)
  must get the matching section — separate commit in `konnextra/Website_`.
- Forum reply to LostWolf once released.
