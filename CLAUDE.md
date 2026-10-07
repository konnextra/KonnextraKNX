# Software_uC — KNX Smart Home Controller

## Project overview

PlatformIO / Arduino project running on a **Seeed XIAO ESP32-C6**. The repository is now
primarily an **Adafruit-style cross-platform Arduino KNX library** (the redesign tracked in
`PLAN.md`), with `examples/BenchTest/BenchTest.ino` as a bench sketch demonstrating its use. The original target
is a KNX wall controller (capacitive touch pads, OLED, RGB backlight); those sensor/display
drivers are not part of the library surface. The thesis button layer (`examples/KNX_Device/`)
was removed — recover it from git history if it is ever needed.

## Build system

PlatformIO, Arduino framework. Build and upload via PlatformIO CLI or the PlatformIO IDE extension.

```
PLATFORMIO_SRC_DIR=examples/BenchTest pio run              # build the bench firmware
PLATFORMIO_SRC_DIR=examples/BenchTest pio run --target upload
pio device monitor   # 115200 baud
pio test -e native   # host unit tests
```

All library code lives under `src/`, flat — this is the Arduino Library Manager–compliant
layout (root `library.properties` + root `src/`), so `pio run` alone has nothing to link;
point `PLATFORMIO_SRC_DIR` at a sketch under `examples/`. Third-party dependencies are declared
in `platformio.ini` and fetched automatically.

Each `examples/` sketch resolves `#include <KonnextraKNX.h>` via `lib_deps = symlink://.` in
`platformio.ini`'s shared `[env]` block: flattening `lib/*/src/` into root `src/` removed the
automatic LDF discovery that used to make the library visible to those builds, so
`symlink://.` links this project's own root (`library.properties` + `src/`) back in as a
library, anchored to the project directory itself. `[env:native]` clears `lib_deps` back to
empty, since it already pulls `src/` in via `test_build_src` — self-referencing there too would
compile every `.cpp` twice and fail to link on duplicate symbols.

## Repository layout

| Path | Contents |
|---|---|
| `src/` | the library's own source — flat, one file per class/module (see Architecture below) |
| `examples/` | standalone `.ino` sketches, one folder each (`DeviceObject`, `StatelessSend`, `CustomKnxObject`, `ExplicitPort`). They mirror the Examples guide in the website repo (`konnextra/Website_`, `docs/content/en/examples.md` and `de/examples.md`) — change one, change the other. `BenchTest` is the exception: it's the hardware bench-test sketch (formerly root `src/main.cpp`), not part of the public showcase, and has no entry there. All of them, `BenchTest` included, are compile-checked by CI's `portability` job (`PLATFORMIO_SRC_DIR=<folder> pio run -e <env>`); check them by hand only for behavior CI can't compile-check (docs staying in sync, runtime correctness). |
| `docs/` | `ApiReference.md`, the landing page of the generated API reference. The user guides are **not** here: they live in the website repo (`konnextra/Website_`, `docs/content/{en,de}/`), in English and German. |
| `reference/` | KNX standard specifications + the TP-UART2 datasheet (PDFs, read-only reference). **Local only, gitignored** — third-party copyrighted documents, and the Library Manager would ship them with every install. Older tags (≤ v0.1.8) still contain them. |
| `.agents/specs/` | design docs from the `superpowers:brainstorming` workflow — the *why* behind completed work. **New specs go here**, not to the skill's default `docs/superpowers/specs/`, which would pollute the user-facing `docs/`. |
| `scripts/` | `bump_version.py` — the release helper |

`CLAUDE.md`, `PLAN.md` and `Changes.md` stay at the repository root. The three do not overlap:
`PLAN.md` is what is still to do, `Changes.md` is what is already done but not yet released, and
`CLAUDE.md` is how the repository works.

## Architecture

Strict layered design — dependencies only flow downward, acyclic (PLAN §12):

```
examples/BenchTest/ ← hardware bench sketch: wires the stack + drives two device objects
src/                ← THE library, flat — everything below is one PlatformIO/Arduino library,
                      one file per class or module, no per-file library.json:
  KonnextraKNX.h    ← the single user include, sole root of the DAG. Defines the KonnextraKNX
                      node class (owns a KnxDriver, built from the physical address).
  KnxObject.h       ← generic device object : IKnxReceiver — group address(es) + KnxDpt,
                      write()/value()/onUpdate(); base class for the intent classes below
  Knx{Light,DimmLight,RGB,Blind,Temperature,Humidity,Time,Date,DateTime,Percent,Char,Float}
                    ← intent classes built on KnxObject, grouped by domain file
                      (KnxLighting.h, KnxCovers.h, KnxClimate.h, KnxDateTime.h, KnxScalars.h)
  KnxCoordinator.h  ← DI core class KnxCoordinator: group send(ga, KnxValue) + intrusive
                      IKnxReceiver registry, point-to-point sendIndividual/sendControl + one
                      optional IKnxDeviceHandler, loop(); injected IKnxDriver*
  KnxDriver.h/.cpp  ← concrete ATTiny/TP-UART UART driver : IKnxDriver (target-only)
  KnxFrame.h/.cpp, KnxReassembler.h/.cpp
                    ← stateless L_Data framing + reassembler; Arduino-free, host-tested
  KnxValue.h, KnxCodec.h/.cpp
                    ← value currency: KnxValue tagged union + symmetric KnxCodec. Pure
  KnxCommon.h       ← umbrella header pulling in the shared vocabulary below
  KnxEnums.h, KnxAddress.h, KnxTelegramTypes.h, KnxInterfaces.h, KnxDebug.h
                    ← shared types + contracts (IKnxDriver / IKnxReceiver / IKnxDeviceHandler,
                      runtime logging switch used by every layer)
examples/           ← standalone .ino sketches mirroring the website's Examples guide (plus BenchTest,
                      which doesn't); NOT in the build, so nothing compiles them automatically
                      except CI's portability matrix
```

Dependency flow is unchanged in spirit: `KonnextraKNX → {KnxDriver, KnxObject, KnxCoordinator,
KnxValue, KnxCommon-tier headers}`, `KnxObject → KnxCoordinator → {KnxFrame, KnxValue, common
headers}`, `KnxDriver → {KnxFrame, common headers}`, `KnxFrame → KnxValue → common headers`.
This is now a **documentation and file-naming convention, not an LDF-enforced one** — flattening
into one `src/` for Arduino Library Manager compliance means PlatformIO no longer tracks
per-module `library.json` dependencies or blocks an upward include at build time. Interfaces
(`IKnxDriver`, `IKnxReceiver`) still live in the lowest-level headers below their consumers, so
the coordinator still never includes the concrete driver or object headers by convention — but
nothing except code review catches a violation now. `KonnextraKNX.h` is still the only header
that pulls in the driver, and nothing includes it — but that convention alone no longer keeps
the Arduino driver out of a host build the way per-folder `lib/` libraries used to: `pio test
-e native`'s `test_build_src = true` compiles **every** `.cpp` under `src/` unconditionally,
regardless of what any test file `#include`s. What actually keeps `KnxDriver.cpp` out is an
explicit `build_src_filter = +<*> -<KnxDriver.cpp>` in `platformio.ini`'s `[env:native]` — add
another Arduino-only `.cpp` to `src/` and it silently joins the native build too unless that
filter is updated to exclude it as well.

No global singletons. Dependencies are injected by constructor pointer/reference.

**User include & construction:** a sketch needs only `#include <KonnextraKNX.h>` and
`KonnextraKNX knx("1.1.5");`. `KonnextraKNX.h` (root of the DAG within the flattened `src/`)
pulls in the driver, the coordinator core, the value currency, and every intent class, then defines the
user-facing **`KonnextraKNX` node class** — a thin
Arduino subclass of `KnxCoordinator` that *owns* a `KnxDriver` and is built from the physical
address, so the user never instantiates or injects a driver (address typed once). The
dependency-injection **core is `KnxCoordinator`** (`KnxCoordinator.h`): Arduino-free, host-testable
with a mock driver, and the type intent objects reference (`KnxCoordinator&`). Advanced users can
inject their own `IKnxDriver` by constructing a `KnxCoordinator` directly.

**Testing:** `pio test -e native` runs the host Unity suite (codec, framing, reassembler,
coordinator, objects) against the Arduino-free layers (needs `test_build_src = true` in
`platformio.ini`'s `[env:native]` to compile `src/` at all — it's no longer pulled in
automatically the way per-folder `lib/` libraries used to be — plus that same env's
`build_src_filter = +<*> -<KnxDriver.cpp>` to keep the Arduino-only driver out of the host
build); `PLATFORMIO_SRC_DIR=examples/BenchTest pio run` builds the firmware.

## Hardware pin map (XIAO ESP32-C6)

| Signal | Pin | Direction |
|---|---|---|
| KNX UART RX | **D7** | IN |
| KNX UART TX | **D6** | OUT |
| NeoPixel data | D3 | OUT |
| I2C SDA | SDA | Shared: MPR121, SSD1306, SHTC3 |
| I2C SCL | SCL | Shared bus |

**RX/TX in that table were the wrong way round until the port injection landed**, and the
correction is counter-intuitive enough to be worth the paragraph. The old bring-up read
`uart.begin(baud, SERIAL_8E1, txPin, rxPin)` with `rxPin = D6, txPin = D7`, while the ESP32
signature is `begin(baud, config, rxPin, txPin)` — the two were passed swapped. Since that
code was hardware-verified, the configuration that actually works is **ESP32 RX = D7, TX = D6**,
and it is the misleading member names, not the wiring, that were wrong. `examples/BenchTest/BenchTest.ino`
reproduces it with `knxPort.setPins(D7, D6)`; do not "correct" that to `(D6, D7)`.

**D6 is also this board's default UART0 TX**, which means the ROM bootloader talks on the KNX
line. `variants/XIAO_ESP32C6/pins_arduino.h` defines `TX = 16` and `D6 = 16` — the same pin. A
UART capture of a boot shows a 35-byte burst at 19200 8E1 roughly three seconds before the
`U_Reset.req`, which is consistent in both size and timing with the ROM banner going out at
115200 (35 bytes read at 19200 ≈ 200 bytes sent at 115200; the three seconds are the
`while (!Serial && millis() < 3000)` in `examples/BenchTest/BenchTest.ino`). So on every reset the transceiver is
fed a burst of wrong-baud traffic before the driver ever opens the port. Most of it dies on
framing and parity errors, but nothing guarantees all of it does — a stray `0x28` consumes the
next two bytes as an address, and a `0x80|i` pattern could start a transmission.

This is inferred from the capture, not proven: to confirm it, sniff D6 at **115200 8N1**
instead and look for readable `ESP-ROM:esp32c6` / `rst:0x1` text. The negative control has
been run and the explanation survived it — a classic ESP32, whose KNX line is UART1 on GPIO
26/27 and therefore nowhere near the console on GPIO 1/3, produced no such burst at all. It is filed as a known trait
rather than a bug because the rig was hardware-verified with it happening, so the ATTiny
evidently tolerates it. The only reliable fix is moving the KNX UART to other GPIOs, which the
soldered bench rig cannot do; disabling the ROM log needs a one-way eFuse and is not worth it.
**The other boards do not have this problem** — on the Nucleo, UNO R4 and Pico `Serial1` is a
separate UART from the console, so the C6 is the exception here, not the pattern.

The KNX pins are **no longer wired into the library**. `KnxDriver` is handed a port it does
not construct, so the pin columns above describe this board's wiring, not a library constant.
On ESP32 the UART keeps whatever pins it already has — call `setPins()` before `knx.begin()`
to move it. The ATTiny `/RESET` line is gone from the hardware; `resetRequest()` therefore has
no fallback left, and an unanswered soft reset is now simply a failed `begin()`.

## Board portability

`KnxDriver` holds a **`Stream*`** for the byte traffic and a **`HardwareSerial*`** only when
it was handed one — that second pointer is what tells `begin()` it may configure the line.
There is no architecture guard anywhere in the driver.

`KNX_DEFAULT_PORT` (top of `KnxDriver.h`) resolves the address-only constructor's port, in
this order: Arduino's own `SERIAL_PORT_HARDWARE_OPEN`, then `HAVE_HWSERIAL1`, then a short
list of cores that always ship `Serial1`. Override it per project with
`-DKNX_DEFAULT_PORT=Serial2`. Where none of them match — the Uno — the constructor is
`= delete`d with an explanatory comment, in **both** `KnxDriver` and `KonnextraKNX`; the second
one is the message users actually see.

Two traps that the compile matrix caught and that will bite again:
- **`HAVE_HWSERIAL1`, not the architecture.** STM32duino *declares* `Serial1` whenever the
  chip has a USART1 but only *defines* it when the sketch sets `ENABLE_HWSERIAL1`. Testing
  `ARDUINO_ARCH_STM32` compiles and then fails at link with "undefined reference to Serial1".
- **`Serial.printf()` is not Arduino API.** ESP32 and STM32duino ship it as an extension; the
  Renesas core does not, and there `Serial` is a `_SerialUSB` with only `print`/`println`/
  `write`. It cost the bench sketch a build on the UNO R4 Minima. Use chained `print()`, or
  `vsnprintf` into a buffer the way `KnxDebug::log()` does. The library was always clean here —
  it is sketches and doc snippets that need watching, and `examples/`, `docs/` and `README.md`
  have been checked and use none.
- **AVR has no C++-style C headers and defaults to C++11.** Use `<stdint.h>`/`<string.h>`,
  never `<cstdint>`/`<cstring>`, and never take the address of a `static constexpr` member
  (that ODR-uses it and needs an out-of-line definition before C++17 — copy it to a local
  first, as `resetRequest()` does).

`ci.yml`'s `portability` job builds every sketch in `examples/` against one env per core
family, and `uno-single-uart` asserts that the address-only constructor **fails** on the Uno.
The envs live in `platformio.ini` and are compile-only — `default_envs` still pins the firmware.

## KNX group addresses in the bench test (`examples/BenchTest/BenchTest.ino`)

| Address | DPT | Direction | Purpose |
|---|---|---|---|
| 0/1/1 | DPT1 | OUT | `lamp` switching command — the GA being toggled |
| 1/1/1 | DPT1 | IN  | `lamp` switching status → `onLampChanged` callback |
| 0/2/1 | DPT5 | IN  | `brightness` status from the dimmer → `onBrightnessChanged` callback |

The `lamp` object is `KnxLight lamp(knx, "0/1/1", "1/1/1")` — the ctor is
`KnxLight(knx, commandGa, statusGa)`, so it *sends* to 0/1/1 and *listens* on 1/1/1. The bench
log confirms it (`TX -> GA 0/1/1`). Note: several comments and `Serial.printf` strings inside
`examples/BenchTest/BenchTest.ino` still say "1/1/1" for the toggled address — those are stale
text, not a wiring change; the code is correct.

Note 0/2/1 is a DPT5 **brightness status**, not a DPT3 relative-dim command GA — so the sketch
uses `KnxLight` + a listen-only `KnxPercent`, not `KnxDimmLight` (whose third GA is a *send*
address for dim steps). No relative-dim command GA is configured on this device.

Group addresses are hardcoded in the sketch (no ETS) — the Adafruit-style trade-off (PLAN §1).
Physical address of this device: **1.1.5**

## Bench-test sketch (`examples/BenchTest/BenchTest.ino`)

`examples/BenchTest/BenchTest.ino` is currently a **hardware bench test**, not the API showcase:
no buttons, one `KnxLight` toggled every 5 s on a `millis()` cadence, plus a listen-only `KnxPercent` for the
dimmer's brightness status, with every status telegram printed over Serial at 115200. Its job is
to exercise the two paths host tests cannot reach — the driver's real transmit path (positive
`L_Data.con`?) and the full receive path (reassemble → parse → match → decode → callback), the
latter across two objects and two DPTs, which also proves registry dispatch selects the right
receiver. `knx.begin()`'s return value is printed at boot, and the file ends with a
symptom→cause guide for reading a bad run.

The prior three-tier API showcase (intent objects / value objects / stateless send, with
`kitchen`, `lamp` and `roomTemp`) is in git history at `e22c388` — restore it from there when
the bench work is done rather than rewriting it.

`delay()` must not be used for the cadence: it stalls `knx.loop()`, so status callbacks arrive
late or get dropped. Objects are declared at global scope — they self-register into the
coordinator's receiver registry on construction and must outlive it (PLAN §6).

**Callback style in the bench sketch:** handlers are **named free functions**, prototyped above
`setup()` and defined below `loop()` (the thesis layout), so `setup()` stays a flat wiring
manifest — one line per binding, no handler bodies inline. `onUpdate` takes a plain
`void(*)(native)`, so a non-capturing lambda works identically; the named form is the idiom the
example teaches because it matches `attachInterrupt(pin, handler, …)` and names the intent. Now
that the bench sketch is itself a `.ino`, its build tooling would auto-generate these forward
declarations — the prototypes are written out anyway, matching the convention every other
`examples/` sketch teaches (see `DeviceObject.ino`, `CustomKnxObject.ino`), not because the
compiler requires it.

## Debug mode

Verbose tracing is a **runtime** switch — no recompile, no `#define`:

```cpp
knx.enableDebugMode(true);   // call before begin() to trace driver bring-up too
```

It prints, all prefixed `[knx]`: driver bring-up and UART traffic, TX frame hexdumps, the raw
`L_Data.con` byte behind each verdict, RX frame hexdumps, frame build/parse (including checksum
mismatches), codec decode failures, per-object decode/cache updates, and **every telegram
dispatched — including telegrams addressed to other devices** (logged as "0 receiver(s)", which
is normal foreign traffic, not an error). Address-validation warnings ride the same switch.

Implementation is `src/KnxDebug.h`: a single library-wide `bool` in an inline
function-local static, plus `log()` / `logBytes()`. Both the `#ifdef ARDUINO` guard and the
runtime check live **inside** those functions, so call sites elsewhere carry no preprocessor
noise and the Arduino-free layers stay Arduino-free — on a host build the bodies compile away
and the native suite is unaffected.

Three properties to know:
- **Library-wide, not per-instance.** The flag is shared; enabling it on any node enables it
  everywhere. This is the agreed, documented exception to the no-globals rule — it is a log
  level, not a service locator, and it is what lets the stateless `KnxFrame`/`KnxCodec`
  namespaces log without an object to hang a flag on.
- **Costs ~2.4 KB flash, 0 RAM** when compiled in and disabled (measured on ESP32-C6); a
  disabled call is one bool test. There is deliberately no compile-time master switch yet — add
  one only if an AVR target ever needs the code stripped entirely.
- **Arguments are still evaluated when disabled.** Guard an expensive call site with
  `if (KnxDebug::isEnabled())`.

Logging is chatty and the printing itself costs time on the receive path, so it can perturb
what it is diagnosing. Establish a clean run first, then switch it on to explain a broken one.

## Documentation and releases

The API reference is generated by **Doxygen 1.17.0** from the public headers listed in the
`Doxyfile`, plus `docs/ApiReference.md` as its landing page, and published to GitHub Pages.

**The guides are not in this repo.** Getting Started, Examples, Supported Boards and the rest
live in the website repo (`konnextra/Website_`, `docs/content/en/` and `docs/content/de/`), and
the website renders them itself. It still fetches the API reference from these Pages, which is
all this repo publishes. A change that makes a guide wrong needs a commit there, in both
languages. Regenerate locally:

```
export PATH="/opt/homebrew/bin:$PATH"   # Homebrew doxygen is not on the default PATH
rm -rf doxygen                          # Doxygen never deletes, only overwrites
doxygen Doxyfile                        # -> doxygen/ (gitignored)
cat doxygen/warnings.txt                # must stay empty
python3 -m http.server 8080 --directory doxygen/html
```

**Purge the output directory first.** Doxygen only overwrites the files it generates this
run — anything renamed or removed since the last run stays behind and looks current. A stale
local `doxygen/` has already held pages for a class that no longer existed and CSS from a
theme that had been deleted. CI is immune (it builds from a fresh checkout); local runs are
not.

**Zero warnings is a hard invariant, not a nicety** — `docs.yml` fails the build if
`doxygen/warnings.txt` is non-empty. Hard-refresh the browser after regenerating; the CSS
filenames don't change, so the old look stays cached.

Three `Doxyfile` settings are load-bearing and easy to break:
- `PREDEFINED = ARDUINO KNX_DEFAULT_PORT=Serial1` — **both required**, and this is the setting
  that fails silently. Doxygen evaluates the preprocessor itself, so any constructor behind an
  `#ifdef` it cannot resolve simply vanishes from the output — with an empty `warnings.txt`.
  Without `ARDUINO` the 25 `String` constructors disappear; without `KNX_DEFAULT_PORT` the
  address-only `KonnextraKNX(addr)` disappears, which is the one every page and example uses.
  **Adding a new `#ifdef` around public API means adding its macro here.**
- `RECURSIVE = NO` with an explicit `INPUT` list — new headers must be added to `INPUT` by
  hand, and `USE_MDFILE_AS_MAINPAGE` must keep pointing at `docs/ApiReference.md`.
- `EXCLUDE_SYMBOLS` hides the internal enums that share `KnxEnums.h` with the user-facing ones.

**Publishing is tag-driven.** `ci.yml` runs tests and the firmware build on branch pushes only;
`docs.yml` fires on a `v*.*.*` tag and runs `verify-version` (tag == `VERSION` ==
`library.properties`) → `test`/`build` against that exact SHA → Doxygen → Pages deploy. Nothing
publishes without a tag.

**`Changes.md` at the root is where release notes are collected.** It is a maintainer file, not
published, and deliberately not in the `Doxyfile` `INPUT` list. Add a line to it *when you make
a change* that would have a user edit their sketch or behave differently on the bus — that is
the whole reason the file exists, because notes reconstructed from `git log` at release time
read like a commit list. Refactors, tests and CI changes do not belong in it.

The release then has one extra step at the front: **rewrite the collected lines into the
Release Notes page in the website repo** (`docs/content/en/releasenotes.md` and its German
twin `de/releasenotes.md`) **under the new version heading, then empty `Changes.md` back to its
template.** The two are not the same text. `Changes.md` is raw and complete; the Release Notes
page is the published one and stays short and curated — only what a user needs, with the
wording worked over. It is a separate commit in the other repo, and pushing it to the
website's `main` deploys it.

```bash
# 1. fold Changes.md into the website's releasenotes.md (en + de) by hand, then empty Changes.md
python3 scripts/bump_version.py 0.1.6   # writes VERSION + library.properties
git diff                                 # sanity-check: only version fields changed
git add VERSION library.properties Changes.md
git commit -m "Bump version to 0.1.6"
git tag v0.1.6
git push && git push --tags              # the tag push is what fires docs.yml
```

`bump_version.py` deliberately never commits, tags or pushes — cutting a release stays an
explicit human step, because the tag push *is* the live deploy. It does count the entries left in
`Changes.md` and print a reminder, since that is the one step with nothing else to catch it.

One-time setup already done, worth knowing if Pages ever stops deploying: enabling Pages
auto-creates a `github-pages` environment whose ref policy allows branches but **not tags**, so
the first tag deploy fails with "not allowed to deploy to github-pages due to environment
protection rules". Fixed under Settings → Environments → `github-pages` → Deployment branches
and tags → Ref type **Tag**, pattern `v*`.

## Physical layer: STKNX behind an ATTiny co-processor

The Siemens TP-UART2 transceiver is replaced by the ST STKNX, a **pure
physical-layer** chip (GPIO, not UART). Rather than bit-bang the STKNX on the main
MCU, an **external ATTiny co-processor** handles all bit-level timing and presents
a **TP-UART-like UART interface** to the main MCU.

Key consequences:
- `KNX_TPUART2` is replaced by a new driver, but the driver still talks **UART**
  to the ATTiny — not GPIO bit-bang on the main MCU.
- **Bit timing, collision detection, and STKNX GPIO drive live on the ATTiny.**
  The main MCU never runs timing-critical ISRs for KNX, so the library stays
  reliable when WiFi/MQTT/Matter stacks share the main core.
- All layers above the physical driver (`KnxFrame`, `KnxCoordinator`, application) keep a
  UART-shaped link-layer interface (send frame / poll for events / TX
  confirmation), so the swap is contained to the driver.
- The ATTiny is expected to return a **transmit confirmation** (TP-UART-style
  `L_Data.con`); the driver must surface real send success/failure instead of
  hard-returning `true` (see `PLAN.md` §9).

> The broader library redesign (Adafruit-style API, `Dpt`/`KnxValue` value types,
> `KnxObject` + intent classes, unified RX registry) is tracked in **`PLAN.md`**.

