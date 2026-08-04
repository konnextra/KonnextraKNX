# PLAN

The library is feature-complete and on `main`, released as `v0.1.7`. Steps 1–4b (the API
reference, the Doxygen site, CI/CD) and Step 6 (the documentation wiki) are done. What is left
is no longer documentation-only:

| Open | What | Blocks |
|---|---|---|
| **Step 5** | bench retest on real hardware — the driver has not been on a bus since the port injection | Step 8, both defects |
| **Step 6 rest** | `docs/Hardware.md` content | user input |
| **Step 7** | wrong Arduino IDE install instructions | — |
| **Step 8** | Arduino Library Manager — moves `src/`, so it waits for Step 5 | Step 5 |
| **Two defects** | read request decoded as 0; dead address-format guards | Step 5 |
| **Step 4c** | `Website_` fetches and styles the published content — other repo | — |

Design rationale for the completed steps lives in `.agents/specs/`; the blow-by-blow is in git
history. Operational knowledge that outlived its step (Doxygen invocation, Doxyfile
constraints, the release process) moved to `CLAUDE.md` — this file tracks only what is still
open.

## Settled, do not re-litigate

- **Scope:** user-facing API + `KnxCoordinator`. Out of scope, left as maintainer docs:
  `KnxFrame`, `KnxDriver`, `KnxReassembler`, `KnxCodec`, `KnxCommon` internals.
- **Style:** reference — `@brief` on every public class/method, `@param`/`@return` where they
  carry information. No usage `@code` examples inside headers (examples live on their own page).
- **Audience: users only.** No maintainer rationale, no `PLAN.md`/`§` references in public doc
  blocks. Plain vocabulary ("true if the bus confirmed the send", not "the `L_Data.con` result").

## Done

| Step | What | Commits | Design doc |
|---|---|---|---|
| 1 | Public API docs rewritten as a user-facing reference (10 headers + 3 user-facing enums; all 25 Arduino `String` ctors newly documented) | `de253bc` | — |
| 2 | Doxygen site: `Doxyfile`, landing page, Examples page | `23215ad` | — |
| 3 | Site-chrome integration — Doxygen output wrapped in the company website's navbar/footer | (follow-ups to `23215ad`) | `.agents/specs/2026-07-21-doxygen-site-integration-design.md` |
| 4a | CI/CD: `ci.yml` on branch pushes, `docs.yml` on `v*.*.*` tags → GitHub Pages; `VERSION` + `scripts/bump_version.py` | `3cf6149`..`e1531b2` | `.agents/specs/2026-07-22-ci-cd-pipeline-design.md` |
| 4b | Theme/chrome stripped — Doxygen is a pure content generator; `doxygen-theme/` (13 files) deleted | `2d6fe03`..`2887b8c` | `.agents/specs/2026-07-22-doxygen-theme-strip-design.md` |

**Step 3 is superseded by 4b, not reverted.** Its chrome-injection mechanism (static navbar/
footer duplicated into the generated pages, then manually `cp -r`'d into `Website_`) is gone;
recover it from git history only if Step 4c wants to see how it was done.

**Live site: https://konnextra.github.io/KonnextraKNX/** — unstyled Doxygen output since
`v0.1.5`, which is exactly what Step 4c is supposed to pick up and dress.

## Step 4c — OPEN: `Website_` fetches the published content and styles it

The only remaining work in this phase, and it happens **in the `Website_` repo, not here**.
`Website_/documentation.php` still points at Step 3's manually copied output, and nothing
styles the now-plain Doxygen HTML.

**Direction agreed with the user** (conversation, no formal design doc yet):

- **Runtime fetch:** `documentation.php` does a **server-side** fetch (PHP
  `file_get_contents`/curl) against `https://konnextra.github.io/KonnextraKNX/` and echoes the
  content fragment into `<main>` below the hero. Navbar, footer and hero stay pure `Website_`
  PHP. Rejected alternatives: CI pushing fragments into `Website_` at build time (reintroduces
  a cross-repo write), and a client-side JS fetch (content invisible without JS, bad for SEO).
- **`Website_` authors its own stylesheet** against Doxygen's default class names (`memitem`,
  `memdoc`, `memname`, …). The palette source of truth is `Website_/style/style.css` — read it
  there rather than copying values around.
- **Content scope:** Getting Started + Examples + a Hardware description + **one combined**
  API Reference page covering all classes (deliberately not per-class routing).

**Open questions — resolve before writing an implementation plan:**

- **Caching for the PHP fetch.** Hitting GitHub on every page view is fragile and slow. Needs
  some cache layer (APCu, file cache with TTL, or HTTP conditional GET) — which one was never
  discussed.
- **Hardware description content.** The mechanism is settled — `docs/Hardware.md` exists and is
  already in Doxygen's `INPUT`, so it flows through the same pipeline as every other page. What
  is *not* settled: who writes the content and from what source. The file is still a stub.
- **Transition for the already-published Step 3 output** sitting in `Website_/main` (commit
  `f153b37`): replaced outright, or does `documentation.php` need a fallback path?

**Not started.** No `Website_` PHP/CSS exists for this. Whoever picks it up should brainstorm it
properly against the `Website_` repo given the open questions above, then write a design doc and
an implementation plan — the same way Steps 3, 4a and 4b were run.

---

# Open work

## Step 5 — OPEN: board portability, hardware half

The code half is done and on `main` (`8252172`): `KnxDriver` no longer constructs a UART, it is
handed a `HardwareSerial&` or a pre-configured `Stream&`. `KNX_DEFAULT_PORT` resolves the
address-only constructor's port and is `= delete`d where no port is free. `ci.yml`'s
`portability` job builds all four examples against six core families on every push, and
`uno-single-uart` asserts the address-only constructor is correctly withheld on the Uno.

**Nothing has been on a KNX bus since the change.** The transmit path has since been checked on
four boards without one, by sniffing the UART (see below); the receive path and the
`L_Data.con` handling still have no hardware evidence at all.

- [ ] **Bench retest on the XIAO ESP32-C6.** This is the gate for everything else — it restores
      the hardware-verified status the driver had before the port injection. `src/main.cpp` is
      already wired for it: the ESP32 branch of the port conditional gives `HardwareSerial(1)`,
      and `setPins(D7, D6)` runs behind `#if defined(ARDUINO_XIAO_ESP32C6)` so it applies on
      this board and nowhere else. Two behaviour changes to watch specifically:
      - `resetRequest()` has **no hardware fallback** any more (the `/RESET` line is gone from
        the hardware), so an unanswered soft reset now fails `begin()` instead of retrying.
      - the port is opened with the **two-argument** `begin(19200, SERIAL_8E1)`; on ESP32 that
        keeps the pins already assigned, which is what `setPins()` sets up. Confirm it really
        talks on D7/D6.
- [x] **Transmit path verified on three further boards by UART sniffing** (4 August 2026), which
      is the method for every board below. A spare board reads the DUT's KNX TX line at 19200 8E1
      and prints what it sees; no transceiver and no bus are involved. It works because
      `KnxCoordinator::begin()` only forwards the driver's verdict and nothing gates on it, so
      an unanswered `begin()` does not suppress the sends. All three ran `src/main.cpp`, whose
      port selection is now a conditional rather than a line to edit per board:

      | Board | Core | KNX port | Pins | Evidence |
      |---|---|---|---|---|
      | Reichelt DEBO JT ESP32 (NodeMCU-32S, WROOM-32) | Espressif, Xtensa LX6 | `HardwareSerial(1)` | UART1 defaults, RX = GPIO 26, TX = GPIO 27 | sniffer |
      | ST Nucleo-L432KC | STM32duino, Cortex-M4 | `Serial1` (USART1) | first `PinMap_UART_TX/RX` entry, RX = PA10 = D0, TX = PA9 = D1 | sniffer |
      | Arduino UNO R4 Minima | Renesas RA4M1, ArduinoCore-API | `Serial1` | `UART1_TX_PIN`/`UART1_RX_PIN`, TX = D1, RX = D0 | sniffer |

      **The R4 Minima is 5 V I/O.** Its TX went through a 1.8 kΩ / 3.3 kΩ divider to reach the
      3.3 V sniffer input; wiring it straight would destroy the pin. Only the one direction
      needs it, since sniffing never drives the DUT.

      **Take the sniffer capture, not just the debug hexdump.** `KNX_VERBOSE`'s `DRV tx frame:`
      line only proves the frame was built and handed to the UART; the sniffer is what proves it
      left the pin, at 19200 8E1, on the pin the pin map claims. On the Nucleo the hexdump was
      available first and already matched, and the capture that followed still mattered — it is
      what confirmed PA9 is really D1.

      `platformio.ini` gained `[env:esp32dev]` and `[env:nucleo_l432kc]` for them — flash envs,
      deliberately not in `ci.yml`. The Nucleo needs `-DENABLE_HWSERIAL1` or `Serial1` is
      declared and never defined; its console stays on USART2/ST-LINK, so the KNX line and the
      debug output do not share pins.

      All three wire streams are **byte-identical**, control octets included, and the bring-up
      gap between `01` and `28 11 05 02` lands at 21–22 ms everywhere (2 × `RESPONSE_TIME_MS`):

      ```
      01                                                        U_Reset.req
      28 11 05 02                                               U_SetAddress + U_State.req
      80 BC 81 11 82 05 83 01 84 01 85 E1 86 00 87 80 48 36     one telegram, paired
         -> BC 11 05 01 01 E1 00 80 36                          checksum OK
      ```

      That is Xtensa LX6, RISC-V and Cortex-M4 — three instruction sets, and with STM32duino a
      second Arduino core rather than a second Espressif chip — producing the same bytes. It
      covers `KNX_DEFAULT_PORT` pins, `SERIAL_8E1`, framing, address
      packing, the checksum and the `millis()` cadence. It covers **neither** the receive path
      **nor** the `L_Data.con` verdict, both of which need a transceiver; both are also
      identical C++ on every core and already host-tested, so they are library risk rather than
      board risk.

      **One difference showed up, in the cadence and nowhere else**, and it sorts by vendor
      rather than by chip:

      | Board | Toggle interval, sniffer-measured |
      |---|---|
      | XIAO ESP32-C6 | 5000 ms |
      | ESP32 WROOM-32 | 5000 ms |
      | Nucleo-L432KC | 5003–5004 ms |
      | UNO R4 Minima | 5004 ms |

      Both Espressif boards land exactly on 5000; both non-Espressif boards sit 600–800 ppm
      above it — **and the sniffer is itself an Espressif board.** The parsimonious reading is
      therefore a clock-reference effect: the Beetle agrees with the DUTs that share its clock
      family and disagrees slightly with the other two, all well inside normal tolerance.

      The competing explanation was that a loop iteration simply takes ~3 ms on those boards,
      which would land as a constant, non-accumulating overshoot because `lastToggle = millis()`
      captures the late value. Two unrelated cores hitting nearly the same figure by coincidence
      makes that the weaker story now, but it is not ruled out. **The cheap discriminator is to
      set `KNX_VERBOSE` false and measure again** — if the offset survives, it is the clock.

      Either way it is an observation, not a defect: 800 ppm is nothing to a 19200 baud UART,
      the sniffer decoded every byte with correct parity on every board, and the bit-level
      timing lives on the ATTiny rather than on the MCU.

      Keep that capture as the reference. Any board whose frame differs by a byte has found a
      real core difference, which is the entire point of the exercise.
- [ ] **First run on the last board** — the Pico. Compile is proven; timing, line settings and
      the transceiver handshake are not. The sniffer above answers the first two without a bus.

      The **Nucleo F401RE is no longer on this list** — the STM32duino board on the bench is the
      L432KC, and it has passed. Its `[env:nucleo_f401re]` in `platformio.ini` and `ci.yml`
      stays: that one is a compile-only proof that the core family still builds, needs no
      hardware, and is what caught the `ENABLE_HWSERIAL1` trap in the first place.
- [x] ~~**Then** bump to `v0.1.7` and tag.~~ **Done out of order, deliberately.** `v0.1.7` was
      cut on 2 August 2026 at the user's explicit direction, to get the rewritten documentation
      published, *before* either bench item above was ticked. So the released driver still has
      not spoken to a bus in its current form. The two boxes above stay open and are now
      **post-release** verification: if the retest finds a problem, it needs a `v0.1.8`, not an
      amended tag.

**Pin order is a trap, do not "fix" it.** The pre-change bring-up read
`uart.begin(baud, SERIAL_8E1, txPin, rxPin)` with `rxPin = D6, txPin = D7`, while the ESP32
signature is `begin(baud, config, rxPin, txPin)` — the two were passed swapped. The wiring that
demonstrably worked is therefore **RX = D7, TX = D6**, and `main.cpp` reproduces exactly that
with `setPins(D7, D6)`. If the bench retest disagrees, the wiring is the thing to re-measure
before the code is changed.

## Step 6 — turn the docs into a wiki: all pages written, published in `v0.1.7`

Surveyed [ArduinoJson](https://arduinojson.org/v7/), the
[FastLED wiki](https://github.com/FastLED/FastLED/wiki) and
[WiFiManager](https://github.com/tzapu/WiFiManager). All three share the same four-block
skeleton — *get it running · look things up · when it breaks · project meta* — and we had only
the first one and half of the second.

Done so far:

| Page | Commit |
|---|---|
| `docs/Troubleshooting.md` | `903de43` |
| `docs/DatapointTypes.md` | `c50619f` |
| `docs/SupportedBoards.md` | `6a90262` |
| `docs/KnxBasics.md` | `a9e442c` |
| `docs/FAQ.md` | `5459f0e` |
| `docs/HowItWorks.md` | `9600134` |
| `docs/ReleaseNotes.md` | `0b0bad2` |
| `docs/Contributing.md` | `b96daa0` |

Then revised in one pass on user feedback (`259ec38`): the serial-port explanation was repeated on
five pages and is now single-sourced on `docs/SupportedBoards.md` with every other page pointing
there; em-dashes went from 103 to 8; pages reordered so `HowItWorks` sits after `FAQ`. A follow-up
(`4f79479`) replaced "bus node" with "bus connection" across the public headers so the generated
reference and the prose pages use the same word. **Do not reintroduce the serial-port explanation
anywhere except `SupportedBoards`.**

Still open:

- [ ] **`docs/Hardware.md` is a placeholder** (`8e4975a`), not a stub any more: it carries the
      parts that are verifiable from the code — transceiver options, the three wires, 19200 8E1,
      the ESP32 `setPins()` story, no reset line, the reference board's D7/D6 — and opens with a
      visible "this page is not finished" banner. **Still needed from the user:** bus supply and
      current draw, signal levels, the Bridge's pinout and mounting, a wiring diagram. Remove the
      banner and the "Still to come" section once they land.
- [x] ~~**`docs/Contributing.md` has no licensing section.**~~ Done, see Step 7. The page now
      closes with a *Licence* section that names BSD 3-Clause and states that a pull request is
      offered under the same terms. That sentence is load-bearing: unlike Apache-2.0 §5, BSD
      3-Clause says nothing at all about contributions, so the page is the only place the terms
      of an incoming contribution are written down.

Each new page must be added to the `Doxyfile` `INPUT` list by hand (`RECURSIVE = NO`), and the
`INPUT` order *is* the navigation order.

## Step 7 — OPEN: project-meta gaps

- [x] ~~**No `LICENSE` file.**~~ **Decided on 3 August 2026: BSD 3-Clause**, copyright
      `Florian Wiesner (Konnextra GesbR)`. `LICENSE` is at the root, all seven `library.json`
      carry `"license": "BSD-3-Clause"`, and README plus `docs/Contributing.md` say so.

      **Why this one, so it is not re-argued.** Nothing constrained the choice: the library has
      no third-party code and no `lib_deps`, so no licence was inherited. Adafruit was the
      reference and is permissive throughout — of 300 org repos, 144 carry their BSD 3-Clause
      "Software License Agreement" (GFX, SSD1306, SHT31, BME280), 20 are MIT, and NeoPixel's
      LGPL-3.0 is a historical outlier. Against MIT, clause 3 adds the one thing that matters
      here: nobody may use the Konnextra name to advertise a derived product. Against
      Apache-2.0, three things decided it — Apache would have meant granting a patent licence
      over exactly the hardware IP the library exists to sell, it is incompatible with GPLv2
      (FSF's position) and so closes off part of the embedded ecosystem, and it wants a `NOTICE`
      file plus a boilerplate header in all 24 sources, which collides with the JSDoc blocks.

      **The copyright holder is the natural person on purpose.** A GesbR is not rechtsfähig
      (§§ 1175 ff ABGB) and cannot hold rights in its own name, and § 10 UrhG knows no corporate
      authorship at all, so the parenthesis names the business context while the person holds
      the right. This also means **relicensing later needs every author's consent** — today all
      24 files are `@authors Florian Wiesner`, and that is worth keeping true.
- [ ] **The Arduino IDE install instructions are wrong, confirmed.** The README and Getting
      Started both tell users to install via *Sketch → Include Library → Add .ZIP Library*. The
      IDE accepts two layouts: 1.0 (headers in the root directory) and 1.5 (`library.properties`
      in the root). This repository has **neither** — no `*.h` and no `library.properties` at
      root, because the code lives in seven PlatformIO libraries under `lib/`. The IDE rejects
      the archive. This is live on the site as of `v0.1.7`. Either correct both pages, or fix it
      properly as part of Step 8, which repairs it as a side effect.

## Step 8 — OPEN: Arduino Library Manager

Wanted, not started. Assessed on 2 August 2026; nothing below has been attempted, it is the
research so the work does not have to start cold.

**Do this after the Step 5 bench retest, not before.** The retest needs `src/main.cpp` buildable
exactly as it is, and Step 8 moves `src/`. Listing a driver in the Library Manager that has never
been on a bus would repeat the trade already made once for `v0.1.7`.

### Why it is worth doing

The name is free and the field is empty. Checked against the live index (9787 libraries):
`Konnextra`, `KonnextraKNX` and `Konnextra KNX` are all unclaimed, and only **two** entries in the
whole index mention KNX at all, `KIMlib` and `KONNEKTING Device Library`. Anyone typing "KNX" into
the Library Manager today finds almost nothing.

### Effort: the submission is 15 minutes, the restructure is half a day

Submission is one line added to `repositories.txt` in a PR to `arduino/library-registry`. Their
bot runs Arduino Lint and merges. Later versions are picked up **from git tags automatically**,
which this repo already produces.

The cost is the repository layout. The IDE accepts the 1.0 layout (headers in the root) or the
1.5 layout (`library.properties` in the root); this repo has neither.

**The cheap part:** all 24 sources have unique basenames and every `#include` is flat
(`"KnxCoordinator.h"`, never `"KnxCommon/KnxDebug.h"`). Arduino's 1.5 layout puts only `src/`
itself on the include path, so moving `lib/*/src/*` into a single root `src/` compiles **with no
source edits**. Had the includes carried subdirectories, this would have been a rewrite.

**What actually breaks** is everything naming a `lib/` path:

| Breaks | Detail |
|---|---|
| `Doxyfile` `INPUT` | 10 explicit `lib/*/src/*.h` entries |
| `scripts/bump_version.py` | 3 refs |
| `docs.yml` `verify-version` | loops `lib/*/library.json` |
| the 7 `library.json` | collapse into one at root |
| `CLAUDE.md` | repository-layout table and architecture section |
| `ci.yml` portability job | **survives unchanged** (uses `PLATFORMIO_SRC_DIR` over `examples/*/`) |

### Checklist

- [ ] Move `lib/*/src/*` (24 files) into a single root `src/`.
- [ ] **`src/` is the collision** — it currently holds the bench sketch. Move that out and point
      `platformio.ini`'s `src_dir` at wherever it lands.
- [ ] Write `library.properties` at root, with `license=BSD-3-Clause` to match `LICENSE` and the
      `library.json` files. **It needs `includes=Konnextra.h`**: without that field
      the IDE's *Include Library* menu inserts an `#include` for all 17 headers instead of the
      one, which breaks the single-include promise every doc page makes.
- [ ] Collapse the seven `library.json` into one, and fix `bump_version.py` and `verify-version`
      to match.
- [ ] Repoint the `Doxyfile` `INPUT` header paths.
- [ ] Update `CLAUDE.md`'s layout and architecture sections.
- [ ] Verify: `pio test -e native`, `pio run`, the CI portability matrix, and an actual
      *Add .ZIP Library* in the Arduino IDE.
- [ ] Fix the install instructions in `README.md` and `docs/GettingStarted.md`, which are wrong
      today (see Step 7).
- [ ] Submit the PR to `arduino/library-registry`.

## Step 4c — OPEN: `Website_` fetches the published content and styles it

Unchanged and still not started; see the section above for the agreed direction and the three
open questions. It happens in the `Website_` repo, not here.

## Two defects found while writing the docs — deliberately not fixed yet

Both are behaviour changes, and the driver is frozen until the Step 5 bench retest. Both are
described accurately in the docs as they behave *today*, so fixing either means updating the
page named with it.

- [ ] **A read request is decoded as a value of 0** (`KnxObject::receive()`, `KnxObject.h`).
      `KnxFrame::parse()` classifies a GroupValueRead correctly (`type = Read`, `inline6Data = 0`),
      `dispatch()` forwards anything with an APCI, and `receive()` never looks at `telegram.type` —
      so `KnxCodec::decode()` returns a *valid* `Dpt1(false)`. Any device polling a group address
      one of your objects listens on silently sets that object's cache to off/zero and fires its
      callback. Minimal fix: ignore `type` other than `Write`/`Response`. The KNX-correct fix is to
      answer with a GroupValueResponse, which is a feature — `KnxFrame::build()` hard-codes
      GroupValueWrite and can build neither a Read nor a Response. Needs a host test.
      **Documented in `docs/FAQ.md`** ("Does it answer read requests from other devices?").
- [ ] **The address format guards are dead code** (`KnxAddress.h`, both `…FromString()`
      functions). `uint8_t x = address.indexOf('.')` turns the `-1` "not found" into 255, so
      `x == -1` is never true and a malformed address never produces its format warning — only
      the range-clamp warnings, or nothing. Fix is `int` instead of `uint8_t`; decide at the same
      time whether the guard should stop parsing rather than only log.
      **Documented in `docs/KnxBasics.md`** ("Bad addresses are corrected, not rejected").

## Smaller open decisions

- **`KnxEnums.h` is mixed-scope** — three documented user-facing enums live alongside untouched
  internal ones, currently handled with `EXCLUDE_SYMBOLS` in the `Doxyfile`. Revisit if the file
  grows.
- **Root `README.md`** exists again but is hand-maintained alongside `docs/GettingStarted.md`.
  The two overlap; if they drift, the README is the one users see first.
- **`examples/` is mirrored by `docs/Examples.md`** — change one, change the other. Nothing
  enforces this.
