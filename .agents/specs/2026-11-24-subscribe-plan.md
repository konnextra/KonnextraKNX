# Stateless receive (`subscribe` + `onTelegram`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a sketch receive group telegrams without a `KnxObject`: `knx.subscribe(ga, dpt, cb)` for sketches, `knx.onTelegram(cb)` for integration layers (Tasmota).

**Architecture:** A new read-only event type `KnxTelegram` wraps the internal `ParsedTelegram`. `KnxCoordinator::dispatch()` gains a single catch-all hook (B) and a fixed subscription table (A), run before the existing receiver registry. Everything stays Arduino-free except the `String` overloads.

**Tech Stack:** C++11 (AVR-safe), PlatformIO, Unity host tests (`pio test -e native`), Doxygen 1.17.

**Spec:** `.agents/specs/2026-11-23-subscribe-design.md` · Decision: `.agents/decisions/0004_stateless-receive-subscribe.md`

## Global Constraints

- No `new`/`delete`, no STL containers. Single exception: the subscription table, one `malloc` (`<stdlib.h>`) on the first `subscribe()`, never freed or resized.
- `<stdint.h>`/`<string.h>`, never `<cstdint>`/`<cstring>` in `src/`; no address of a `static constexpr` member.
- Methods camelCase; default slot count `#define KNX_DEFAULT_SUBSCRIPTION_SLOTS 64` (all boards), changed at run time only via `setSubscriptionSlotLimit()`.
- `KnxTelegram.h` and the coordinator stay Arduino-free; `String` overloads only inside `#ifdef ARDUINO`.
- Every public method gets a user-facing `@brief` (plain words, no PLAN/§ references, no `@code`).
- Doxygen: new public header added to `Doxyfile` `INPUT`; `doxygen/warnings.txt` stays empty.
- Commits: subject line only, no AI attribution.
- PlatformIO binary: `~/.platformio/penv/bin/pio` (not on PATH).

## Review Focus

1. **Callback unsubscribes itself (or another entry on the same GA) during dispatch** → the remaining entries for that telegram still fire, no skipped or double call. Test in Task 3.
2. **Read request on a subscribed GA** → subscribe callback does *not* fire; `onTelegram` does, with `isRead()` true and `decode()` invalid. Tests in Tasks 2 and 3.
3. **Telegram whose payload does not fit the subscribed DPT** (e.g. 2-byte DPT9 arriving on a DPT1 subscription) → callback skipped, other matching entries and the registry still served. Test in Task 3.
4. **`subscribe`/`onTelegram` with `nullptr` callback** → `subscribe` returns `false` and stores nothing; `onTelegram(nullptr)` removes the hook (must compile without ambiguity). Tests in Tasks 2 and 3.
5. **`KnxTelegram` used after the callback returns** → not testable; the `@brief` of `KnxTelegram` must say it is only valid inside the callback and to copy `value()` out.

---

### Task 1: `KnxTelegram` event type + shared telegram decode

**Files:**
- Create: `src/KnxTelegram.h`
- Modify: `src/KnxCodec.h`, `src/KnxCodec.cpp` (add `decodeTelegram`), `src/KnxObject.h` (`receive()` uses it), `Doxyfile` (`INPUT` += `src/KnxTelegram.h`), `src/KnxDebug.h:15` (stale "KnxTelegram" in the layer list → "KnxFrame")
- Test: `test/test_codec/` (decodeTelegram), new `test/test_telegram/test_telegram.cpp`

**Interfaces:**
- Produces:
  - `KnxValue KnxCodec::decodeTelegram(KnxDpt dpt, const ParsedTelegram& t)` — picks `&t.inline6Data`/1 when `KnxCodec::isInline6(dpt)`, else `t.payload`/`t.payloadLength`, then `decode()`. No Read check.
  - `class KnxTelegram` in `KnxTelegram.h`:
    - `explicit KnxTelegram(const ParsedTelegram& t, KnxValue value = KnxValue())`
    - `uint16_t groupAddress(void) const` (packed), `PhysicalAddress source(void) const`, `GroupValueType type(void) const`, `bool isRead(void) const`
    - `KnxValue value(void) const` — the value passed in (invalid by default)
    - `KnxValue decode(KnxDpt dpt) const` — invalid `KnxValue` when `isRead()`, else `decodeTelegram`
    - `const uint8_t* payload(void) const`, `uint8_t payloadLength(void) const` — when the parsed payload length is 0 return `&inline6Data` and 1
  - `typedef void (*KnxTelegramCallback)(const KnxTelegram&);`
  - `typedef void (*KnxTelegramContextCallback)(const KnxTelegram&, void* context);`

- [ ] **Step 1: Write failing tests**

Test helper (put in `test_telegram.cpp`, reused in Tasks 2–3): build a frame with `KnxFrame::build(src, ga, value, f, sizeof f)`, then for Read/Response rewrite octet 7: `f[7] = (f[7] & 0x3F) | cmdBits` (Read `0x00` and also clear data bits → `f[7] = 0x00`; Response `0x40`), and recompute `f[n-1] = KnxFrame::checksum(f, n-1)`; parse with `KnxFrame::parse`.

Tests:
- `test_decode_telegram_inline_dpt1`: Write `Dpt1(true)` → `decodeTelegram(DPT1, p).asBool() == true`.
- `test_decode_telegram_payload_dpt9`: Write `Dpt9(21.5f)` → `asFloat()` within 0.01 of 21.5.
- `test_telegram_accessors`: source `{1,1,7}`, GA 0/3/0 → `groupAddress() == packGroupAddress({0,3,0})`, `source().device == 7`, `type() == Write`, `!isRead()`, `!value().isValid()`.
- `test_telegram_decode_read_is_invalid`: Read frame → `isRead()`, `!decode(DPT1).isValid()`.
- `test_telegram_decode_response`: Response `Dpt1(true)` → `type() == Response`, `decode(DPT1).asBool()`.
- `test_telegram_value_passthrough`: `KnxTelegram(p, Dpt9(3.0f)).value().asFloat() == 3.0`.
- `test_telegram_raw_inline`: Write `Dpt1(true)` → `payloadLength() == 1`, `payload()[0] & 0x01`.

- [ ] **Step 2:** `~/.platformio/penv/bin/pio test -e native -f test_telegram -f test_codec` → FAIL (missing header/function).
- [ ] **Step 3:** Implement `decodeTelegram`, `KnxTelegram.h` (inline, header-only), switch `KnxObject::receive()` to `KnxCodec::decodeTelegram(objectDpt, telegram)` — behaviour unchanged, Reads still decoded there. Doxygen class `@brief` includes the lifetime rule (Review Focus 5).
- [ ] **Step 4:** `~/.platformio/penv/bin/pio test -e native` → all suites PASS (incl. unchanged `test_object`).
- [ ] **Step 5:** Commit `Add KnxTelegram event type`.

### Task 2: `onTelegram` catch-all hook

**Files:**
- Modify: `src/KnxCoordinator.h`, `src/KnxCoordinator.cpp` (`dispatch()`)
- Test: `test/test_coordinator/test_coordinator.cpp`

**Interfaces:**
- Consumes: `KnxTelegram`, both callback typedefs (Task 1).
- Produces:
  - `void onTelegram(KnxTelegramCallback callback)` — `nullptr` removes the hook
  - `void onTelegram(KnxTelegramContextCallback callback, void* context)` — no default for `context`, so `onTelegram(nullptr)` resolves to the first overload
  - private members: one callback slot that remembers which shape it holds + `void* p_telegramContext`

- [ ] **Step 1: Write failing tests** (MockDriver as in the file; helper from Task 1 copied in)
  - `test_on_telegram_sees_foreign_ga`: hook set, no receivers; Write to 0/7/7 → hook called once, `t.groupAddress()` matches.
  - `test_on_telegram_sees_read`: Read on 0/3/0 → hook called, `t.isRead()`, `!t.decode(DPT1).isValid()`.
  - `test_on_telegram_context`: context overload with `&counter` → callback increments `*(int*)ctx`; counter == 1.
  - `test_on_telegram_replace_and_remove`: set A, set B → only B fires; `onTelegram(nullptr)` → nothing fires.
  - `test_on_telegram_and_registry_both_fire`: hook + `TestReceiver` on same GA → both counts 1.
- [ ] **Step 2:** `pio test -e native -f test_coordinator` → FAIL.
- [ ] **Step 3:** Implement; in `dispatch()` call the hook first (after the existing `hasApci` guard), then the registry. Construct `KnxTelegram tg(telegram)` once per dispatch.
- [ ] **Step 4:** `pio test -e native` → PASS.
- [ ] **Step 5:** Commit `Add onTelegram hook`.

### Task 3: `subscribe` / `unsubscribe` table

**Files:**
- Modify: `src/KnxCoordinator.h`, `src/KnxCoordinator.cpp`
- Test: `test/test_coordinator/test_coordinator.cpp`

**Interfaces:**
- Consumes: Task 1 types, Task 2 dispatch ordering.
- Produces:
  - `#define KNX_DEFAULT_SUBSCRIPTION_SLOTS 64` (top of `KnxCoordinator.h`)
  - `bool setSubscriptionSlotLimit(uint16_t total)` — `true` and stores `total` while no table exists; `false` once the first `subscribe()` allocated it
  - `bool subscribe(uint16_t groupAddress, KnxDpt dpt, KnxTelegramCallback callback)`
  - `bool subscribe(uint16_t groupAddress, KnxDpt dpt, KnxTelegramContextCallback callback, void* context)`
  - `void unsubscribe(uint16_t groupAddress)` — frees every slot on that GA, no compaction
  - private: `Subscription* p_subscriptions = nullptr`, `uint16_t subscriptionLimit = KNX_DEFAULT_SUBSCRIPTION_SLOTS`; slot = GA (`uint16_t`), `KnxDpt`, `uint8_t` flags (used, has-context), callback union, `void*` context. Allocated with `calloc(limit, sizeof(Subscription))` on the first `subscribe()` (zeroed = all slots free); `nullptr` result → `subscribe` returns `false` and logs. Limit 0 → nothing allocated, `subscribe` returns `false`.

- [ ] **Step 1: Write failing tests**
  - `test_subscribe_fires_on_write`: DPT1 on 0/3/0, Write true → called once, `t.value().asBool()`, `t.groupAddress()` correct.
  - `test_subscribe_fires_on_response`: Response → called once.
  - `test_subscribe_ignores_read`: Read → not called (Review Focus 2).
  - `test_subscribe_other_ga_ignored`: Write to 0/3/1 → not called.
  - `test_subscribe_full_returns_false`: 64 subscribes return true, the 65th returns false.
  - `test_slot_limit_before_first_subscribe`: `setSubscriptionSlotLimit(100)` → true; 100 subscribes true, 101st false.
  - `test_slot_limit_after_subscribe_rejected`: one `subscribe`, then `setSubscriptionSlotLimit(100)` → false; table stays at 64.
  - `test_slot_limit_zero`: limit 0 → `subscribe` returns false.
  - `test_subscribe_nullptr_rejected`: returns false; then 64 further subscribes still succeed (nothing stored).
  - `test_subscribe_shared_ga_both_fire`: two callbacks on one GA → each once.
  - `test_unsubscribe_frees_slot`: fill table, `unsubscribe` one GA, `subscribe` again → true; unsubscribed callback no longer fires.
  - `test_unsubscribe_inside_callback`: entries X, Y on the same GA; X calls `unsubscribe` on that GA → Y still fires once for that telegram, neither fires on the next (Review Focus 1). Coordinator pointer via a file-static.
  - `test_subscribe_decode_mismatch_skipped`: DPT1 subscription + DPT9 subscription on same GA, Write `Dpt9(21.5f)` → DPT9 callback fires, registry receiver on same GA still receives; DPT1 callback: assert whatever `decodeTelegram` yields is consistent — if `decode(DPT1)` of a 2-byte payload returns invalid the callback must not fire (Review Focus 3). Implementer: check the codec's actual result first and assert the skip only for an invalid decode.
  - `test_subscribe_context`: context overload passes `ctx` through.
  - `test_hook_subscribe_object_order`: hook, subscription and `TestReceiver` on one GA each record a sequence number from a shared counter → order hook < subscription < receiver.
- [ ] **Step 2:** `pio test -e native -f test_coordinator` → FAIL.
- [ ] **Step 3:** Implement. Dispatch: hook → for each used slot with matching GA and type Write/Response: `decodeTelegram`, skip + `KnxDebug::log` on invalid, else call with `KnxTelegram(telegram, decoded)` → registry. The debug line's count includes subscription hits; log "SUB !! table full" on overflow.
- [ ] **Step 4:** `pio test -e native` → PASS.
- [ ] **Step 5:** Commit `Add subscribe table`.

### Task 4: String overloads, example, docs, portability

**Files:**
- Modify: `src/KnxCoordinator.h` (`#ifdef ARDUINO` block), `keywords.txt`, `Changes.md`, `README.md` (only if it states that receiving needs an object), `.agents/STATUS.md`
- Create: `examples/Subscribe/Subscribe.ino`

**Interfaces:**
- Produces (Arduino only): `bool subscribe(String ga, KnxDpt dpt, KnxTelegramCallback cb)`, `bool subscribe(String ga, KnxDpt dpt, KnxTelegramContextCallback cb, void* context)`, `void unsubscribe(String ga)` — all via `packedGroupAddressFromString`.

- [ ] **Step 1:** Add the overloads. Update the `send(String…)` `@brief` ("This only sends; to receive, use a device object") to point to `subscribe()`.
- [ ] **Step 2:** Write `examples/Subscribe/Subscribe.ino` in the style of `StatelessSend.ino` (header comment, 4-space indent, named free functions prototyped above `setup()`, no `Serial.printf`): `subscribe("0/3/0", KnxDpt::DPT1, onLightStatus)` printing on/off, and `subscribe("0/4/2", KnxDpt::DPT9, onTemperature)` printing the float with `Serial.print`.
- [ ] **Step 3:** `keywords.txt`: `KnxTelegram` KEYWORD1; `subscribe`, `unsubscribe`, `onTelegram` KEYWORD2 (tab-separated, match the file). `Changes.md`: one user-facing line for `subscribe`/`unsubscribe`/`setSubscriptionSlotLimit` (KEYWORD2 too), one for `onTelegram`/`KnxTelegram`, under the existing 0.2.0 section format.
- [ ] **Step 4: Verify**
  - `~/.platformio/penv/bin/pio test -e native` → all PASS
  - `PLATFORMIO_SRC_DIR=examples/Subscribe ~/.platformio/penv/bin/pio run -e seeed_xiao_esp32c6` and the Mega env from `platformio.ini` → SUCCESS
  - `PLATFORMIO_SRC_DIR=examples/BenchTest ~/.platformio/penv/bin/pio run` → SUCCESS
  - `export PATH="/opt/homebrew/bin:$PATH"; rm -rf doxygen; doxygen Doxyfile; cat doxygen/warnings.txt` → empty, and `doxygen/html` contains `classKnxTelegram.html` and `subscribe` on the KnxCoordinator page.
- [ ] **Step 5:** Commit `Add Subscribe example and docs`. Update `.agents/STATUS.md` (done / open: website Examples en+de section, hardware check subscribing to 0/1/1 from the button, forum reply to LostWolf) and commit `Update status`.

## Not in this plan (follow-ups)

- Website `konnextra/Website_`: Examples guide section for `Subscribe` in `docs/content/en/examples.md` and `de/examples.md` (CLAUDE.md requires examples and guide to match).
- Hardware check on the test case: subscribe to 0/1/1, press button 1.1.1.
- Forum reply to LostWolf after the 0.2.0 release.
