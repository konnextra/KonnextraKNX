# PLAN

The library is feature-complete and on `main`, released as `v0.1.7`. Steps 1–4b (the API
reference, the Doxygen site, CI/CD) and Step 6 (the documentation wiki) are done. What is left
is no longer documentation-only:

| Open | What | Blocks |
|---|---|---|
| **Step 5** | bench retest on real hardware — the driver has not been on a bus since the port injection | Step 8, both defects |
| **Step 6 rest** | `docs/Hardware.md` content, licensing section | user input |
| **Step 7** | support mailbox, `LICENSE`, wrong Arduino IDE install instructions | user input (LICENSE) |
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

**Everything below is compile-verified only. Nothing has been on a bus since the change.**

- [ ] **Bench retest on the XIAO ESP32-C6.** This is the gate for everything else — it restores
      the hardware-verified status the driver had before the port injection. `src/main.cpp` is
      already wired for it (`HardwareSerial knxPort(1)` + `knxPort.setPins(D7, D6)`).
      Two behaviour changes to watch specifically:
      - `resetRequest()` has **no hardware fallback** any more (the `/RESET` line is gone from
        the hardware), so an unanswered soft reset now fails `begin()` instead of retrying.
      - the port is opened with the **two-argument** `begin(19200, SERIAL_8E1)`; on ESP32 that
        keeps the pins already assigned, which is what `setPins()` sets up. Confirm it really
        talks on D7/D6.
- [ ] **First run on the newly ordered boards** — Nucleo F401RE, UNO R4 Minima, Pico. Compile is
      proven; timing, line settings and the transceiver handshake are not.
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
- [ ] **`docs/Contributing.md` has no licensing section**, because there is no `LICENSE` — see
      Step 7. Add one when that is decided; a contributing page that says nothing about the
      terms a contribution is made under is incomplete.

Each new page must be added to the `Doxyfile` `INPUT` list by hand (`RECURSIVE = NO`), and the
`INPUT` order *is* the navigation order.

## Step 7 — OPEN: project-meta gaps

- [ ] **`support@konnextra.at` is published but unconfirmed.** `docs/Troubleshooting.md` tells
      users to write there, and that is live as of `v0.1.7`. Confirm the mailbox actually
      receives, or change the page.
- [ ] **No `LICENSE` file.** The website and the README both say "Free / Open-Source"; the repo
      says nothing, and `library.json` has no `license` field. **Needs the user's decision** —
      this is a legal call, and there is a commercial product next to it.
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
- [ ] Write `library.properties` at root. **It needs `includes=Konnextra.h`**: without that field
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
