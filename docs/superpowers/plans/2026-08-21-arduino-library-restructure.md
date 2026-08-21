# Arduino Library Manager Restructure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restructure this repo's root so it satisfies the Arduino Library Manager registry's hard requirement — `library.properties` at repo root, one library per repo — while keeping `pio test -e native` and the firmware/portability CI matrix working.

**Architecture:** Flatten the 7 separate PlatformIO libraries under `lib/*/src/` into one flat root `src/` directory (all 24 source files share unique basenames, so existing bare `#include "X.h"` lines keep resolving unchanged — Arduino/PlatformIO only add the top of `src/` to the include path, not subfolders). Move today's `src/main.cpp` bench sketch to `examples/BenchTest/BenchTest.ino` so root `src/` means only "the library" from here on, matching what Arduino's spec expects there. Add `library.properties` + `keywords.txt` at root. `lib/` is deleted entirely — the internal per-layer PlatformIO-library boundary (separate `library.json`, separate declared dependencies) goes away; the layering itself stays as documentation/naming convention, not an LDF-enforced constraint. This is the tradeoff already agreed with the user for Option A.

**Tech Stack:** PlatformIO (Arduino framework + native/Unity host tests), Doxygen 1.17.0, GitHub Actions.

## Global Constraints

- Every existing `#include "X.h"` line must keep resolving without modification — verified by unique basenames (checked: no collisions across the 24 files being merged).
- `pio test -e native` must still pass, unchanged in behavior (only config wiring changes, no test file edits).
- The firmware still must build for `seeed_xiao_esp32c6` and pass the portability/`uno-single-uart` CI jobs — just via `PLATFORMIO_SRC_DIR=examples/BenchTest` instead of the bare `pio run`.
- License stays BSD-3-Clause throughout (`LICENSE`, `library.properties`, and now per-file SPDX headers) — do not introduce a different license anywhere.
- No behavioral change to any class, method, or the bench sketch's logic — this is a pure file-location + build-config + doc restructure.
- Repo URL for `library.properties`: `https://github.com/konnextra/KonnextraKNX` (confirmed via `git remote -v`).
- Current version: `0.1.7` (from `VERSION`) — carry it into the new `library.properties`.

---

### Task 1: Flatten `lib/*/src` into root `src/`, add SPDX headers, remove `lib/`

**Files:**
- Move (via `git mv`): all 24 files from `lib/{KnxCommon,KnxCoordinator,KnxDriver,KnxObject,KnxTelegram,KnxValue,KonnextraKNX}/src/*` into `src/`
- Remove: `lib/` entirely (7 `library.json` files + `lib/README`)
- Modify: each of the 24 moved files gets one new line — an SPDX header

**Interfaces:**
- Produces: root `src/` containing exactly these 24 files, flat (no subfolders): `KnxAddress.h`, `KnxCommon.h`, `KnxDebug.h`, `KnxEnums.h`, `KnxInterfaces.h`, `KnxTelegramTypes.h` (from `KnxCommon`); `KnxCoordinator.h`, `KnxCoordinator.cpp` (from `KnxCoordinator`); `KnxDriver.h`, `KnxDriver.cpp` (from `KnxDriver`); `KnxClimate.h`, `KnxCovers.h`, `KnxDateTime.h`, `KnxLighting.h`, `KnxObject.h`, `KnxScalars.h` (from `KnxObject`); `KnxFrame.h`, `KnxFrame.cpp`, `KnxReassembler.h`, `KnxReassembler.cpp` (from `KnxTelegram`); `KnxCodec.h`, `KnxCodec.cpp`, `KnxValue.h` (from `KnxValue`); `KonnextraKNX.h` (from `KonnextraKNX`). 6 + 2 + 2 + 6 + 4 + 3 + 1 = 24.

- [ ] **Step 1: Move every source file into a flat root `src/`**

```bash
mkdir -p src
for f in lib/*/src/*; do
  git mv "$f" "src/$(basename "$f")"
done
git rm -r lib/
```

- [ ] **Step 2: Verify no filename collisions occurred and the directory is flat**

Run: `ls src/ | wc -l && ls src/*/  2>&1`
Expected: count is `24`; the second command errors with "No such file or directory" (proof there are no subfolders).

- [ ] **Step 3: Add an SPDX header line to every moved file**

Header files (`.h`) get `#pragma once` on line 1, so the SPDX line becomes line 2:

```bash
for f in src/*.h; do
  sed -i '' '1a\
// SPDX-License-Identifier: BSD-3-Clause
' "$f"
done
```

Source files (`.cpp`) have no `#pragma once` — they start directly with the `/**` JSDoc block per the project's C++ style guide — so the SPDX line becomes the new line 1:

```bash
for f in src/*.cpp; do
  sed -i '' '1i\
// SPDX-License-Identifier: BSD-3-Clause
' "$f"
done
```

- [ ] **Step 4: Spot-check two files by hand**

Run: `head -3 src/KnxDriver.h && echo --- && head -3 src/KnxCoordinator.cpp`
Expected:
```
#pragma once
// SPDX-License-Identifier: BSD-3-Clause
/**
---
// SPDX-License-Identifier: BSD-3-Clause
/**
```

- [ ] **Step 5: Confirm every #include still resolves — try a native test build**

This will currently fail (Task 4 hasn't wired `test_build_src` yet) — that's expected here; just confirm the failure is about the test runner not finding `src/`, not a missing header:

Run: `pio test -e native 2>&1 | tail -30`
Expected: failure mentions the test runner/linker missing symbols from `src/`, not `fatal error: X.h: No such file or directory`. If you see a missing-header error, stop — that means a bare include didn't resolve and Global Constraint #1 was violated; do not proceed to Task 2 until it's fixed.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "Flatten lib/*/src into root src/ for Arduino Library Manager compliance"
```

---

### Task 2: Add root `library.properties` and `keywords.txt`

**Files:**
- Create: `library.properties`
- Create: `keywords.txt`

**Interfaces:**
- Consumes: `VERSION` file content (`0.1.7`), repo URL (`https://github.com/konnextra/KonnextraKNX`)
- Produces: the two files the Arduino Library Manager registry validator checks for at repo root

- [ ] **Step 1: Write `library.properties`**

```properties
name=KonnextraKNX
version=0.1.7
author=Florian Wiesner <wiesner.florian@gmx.at>
maintainer=Florian Wiesner <wiesner.florian@gmx.at>
sentence=Talk to a KNX bus from an Arduino sketch — no ETS, no KNX-stack expertise required.
paragraph=Describes devices by what they are (KnxLight, KnxDimmLight, KnxRGB, KnxBlind, KnxTemperature, KnxHumidity, ...) instead of by datapoint tables. Handles KNX telegram framing, datapoint encoding, and bus timing through an ATTiny/STKNX co-processor link, exposed over a single header include.
category=Communication
url=https://github.com/konnextra/KonnextraKNX
architectures=*
includes=KonnextraKNX.h
```

- [ ] **Step 2: Write `keywords.txt`** (tab-separated: `KEYWORD<TAB>TOKENTYPE`; blank line between groups is cosmetic only)

```
#######################################
# Syntax Coloring Map for KonnextraKNX
#######################################

#######################################
# Datatypes (KEYWORD1)
#######################################

KonnextraKNX	KEYWORD1
KnxCoordinator	KEYWORD1
KnxObject	KEYWORD1
KnxValue	KEYWORD1
KnxLight	KEYWORD1
KnxDimmLight	KEYWORD1
KnxRGB	KEYWORD1
KnxBlind	KEYWORD1
KnxTemperature	KEYWORD1
KnxHumidity	KEYWORD1
KnxTime	KEYWORD1
KnxDate	KEYWORD1
KnxDateTime	KEYWORD1
KnxPercent	KEYWORD1
KnxChar	KEYWORD1
KnxFloat	KEYWORD1
IKnxDriver	KEYWORD1
IKnxReceiver	KEYWORD1
IKnxDeviceHandler	KEYWORD1

#######################################
# Methods and Functions (KEYWORD2)
#######################################

begin	KEYWORD2
loop	KEYWORD2
reset	KEYWORD2
send	KEYWORD2
sendIndividual	KEYWORD2
sendControl	KEYWORD2
registerReceiver	KEYWORD2
unregisterReceiver	KEYWORD2
setDeviceHandler	KEYWORD2
enableDebugMode	KEYWORD2
onUpdate	KEYWORD2
on	KEYWORD2
off	KEYWORD2
set	KEYWORD2
toggle	KEYWORD2
isOn	KEYWORD2
brighter	KEYWORD2
darker	KEYWORD2
stopDimming	KEYWORD2
setBrightness	KEYWORD2
```

- [ ] **Step 3: Sanity-check tabs, not spaces, separate the columns**

Run: `cat -A library.properties keywords.txt | grep -c '\^I'`
Expected: a nonzero count (each `keywords.txt` data line has exactly one tab; `cat -A` prints tabs as `^I`).

- [ ] **Step 4: Commit**

```bash
git add library.properties keywords.txt
git commit -m "Add library.properties and keywords.txt for Arduino Library Manager"
```

---

### Task 3: Move the bench sketch out of root `src/`

**Files:**
- Move: `src/main.cpp` → `examples/BenchTest/BenchTest.ino`
- Modify: the `@name` line in the moved file's header comment

**Interfaces:**
- Consumes: nothing new — `#include <KonnextraKNX.h>` still resolves against the flattened root `src/` from Task 1, exactly as it did against the old `lib/KonnextraKNX/src/`.
- Produces: root `src/` now contains only library code (no `setup()`/`loop()` entry point) — this is the change that makes root `src/` mean "the library," matching what the Arduino spec expects there.

- [ ] **Step 1: Move and rename**

```bash
mkdir -p examples/BenchTest
git mv src/main.cpp examples/BenchTest/BenchTest.ino
```

- [ ] **Step 2: Update the file's own header comment to match its new name**

In `examples/BenchTest/BenchTest.ino`, change:
```
 * @name main.cpp
```
to:
```
 * @name BenchTest.ino
```
Leave everything else in the file untouched — no logic, pin, or comment changes beyond this one line.

- [ ] **Step 3: Confirm root `src/` no longer has an entry point**

Run: `ls src/*.cpp src/*.ino 2>&1`
Expected: lists the 5 `.cpp` files from Task 1 (`KnxCoordinator.cpp`, `KnxDriver.cpp`, `KnxFrame.cpp`, `KnxReassembler.cpp`, `KnxCodec.cpp`); `src/*.ino` errors "No such file or directory". No `main.cpp` anywhere under `src/`.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "Move bench sketch to examples/BenchTest so root src/ is library-only"
```

---

### Task 4: Wire `platformio.ini` for the new layout

**Files:**
- Modify: `platformio.ini`

**Interfaces:**
- Consumes: the `[env:native]` section (currently just `platform`, `test_framework`, `build_flags`)
- Produces: native tests linking against the now-flat `src/` again; firmware/portability commands documented with the required `PLATFORMIO_SRC_DIR` override

- [ ] **Step 1: Add `test_build_src` to the native env**

In `platformio.ini`, find:
```ini
[env:native]
platform = native
test_framework = unity
build_flags = -std=c++14
```
Replace with:
```ini
[env:native]
platform = native
test_framework = unity
build_flags = -std=c++14
; src/ is now the library itself (no main.cpp/entry point), and `pio test` does not build
; src/ by default. Without this, none of the KnxCoordinator/KnxFrame/... symbols the test/
; files reference would be compiled in.
test_build_src = true
```

- [ ] **Step 2: Update the top-of-file comment now that `pio run` alone no longer builds firmware**

Find:
```ini
[platformio]
; `pio run` builds only the firmware; the host env is for `pio test -e native`.
default_envs = seeed_xiao_esp32c6
```
Replace with:
```ini
[platformio]
; root src/ is the library itself, not firmware — `pio run` alone has no entry point to link.
; Build the bench firmware with:  PLATFORMIO_SRC_DIR=examples/BenchTest pio run
; The host env is for `pio test -e native`.
default_envs = seeed_xiao_esp32c6
```

- [ ] **Step 3: Run native tests**

Run: `pio test -e native`
Expected: PASS, same test count as before this restructure (`test_frame`, `test_coordinator`, `test_codec`, `test_object`, `test_reassembler`).

- [ ] **Step 4: Run the bench firmware build with the new override**

Run: `PLATFORMIO_SRC_DIR=examples/BenchTest pio run -e seeed_xiao_esp32c6`
Expected: PASS (builds `BenchTest.ino` against the flattened `src/`, same as `main.cpp` did before).

- [ ] **Step 5: Commit**

```bash
git add platformio.ini
git commit -m "Wire platformio.ini for the flattened library-only src/ layout"
```

---

### Task 5: Update CI workflows

**Files:**
- Modify: `.github/workflows/ci.yml`
- Modify: `.github/workflows/docs.yml`

**Interfaces:**
- Consumes: the `PLATFORMIO_SRC_DIR=examples/BenchTest` pattern from Task 4, already used elsewhere in `ci.yml` for the portability/`uno-single-uart` jobs
- Produces: `firmware-build` (ci.yml) and `build`/`verify-version` (docs.yml) jobs passing again; a new `arduino-lint` job matching the registry's own recommendation (its FAQ explicitly names `arduino-lint-action` for CI)

- [ ] **Step 1: Fix `ci.yml`'s `firmware-build` job**

Find:
```yaml
  firmware-build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: ./.github/actions/setup-pio
      - run: pio run -e seeed_xiao_esp32c6
```
Replace with:
```yaml
  firmware-build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: ./.github/actions/setup-pio
      - run: PLATFORMIO_SRC_DIR=examples/BenchTest pio run -e seeed_xiao_esp32c6
```

- [ ] **Step 2: Note the `portability` job needs no change**

Its loop is `for ex in examples/*/`, so it will now also build `BenchTest` on every portability-matrix board automatically — this is correct and desirable (proves the bench sketch's `KNX_DEFAULT_PORT` fallback path works everywhere the address-only constructor does). No edit needed here; just be aware of it for Step 5's verification.

- [ ] **Step 3: Add an `arduino-lint` job to `ci.yml`**

Append a new job (after `uno-single-uart`):
```yaml

  arduino-lint:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: arduino/arduino-lint-action@v1
        with:
          library-manager: update
          compliance: strict
```

- [ ] **Step 4: Fix `docs.yml`'s `verify-version` job**

Find:
```yaml
          for f in lib/*/library.json; do
            LIB_VERSION="$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['version'])" "$f")"
            if [ "$LIB_VERSION" != "$FILE_VERSION" ]; then
              echo "::error::$f version ($LIB_VERSION) does not match VERSION file ($FILE_VERSION)"
              exit 1
            fi
          done

          echo "All versions agree: $FILE_VERSION"
```
Replace with:
```yaml
          LP_VERSION="$(grep '^version=' library.properties | cut -d= -f2)"
          if [ "$LP_VERSION" != "$FILE_VERSION" ]; then
            echo "::error::library.properties version ($LP_VERSION) does not match VERSION file ($FILE_VERSION)"
            exit 1
          fi

          echo "All versions agree: $FILE_VERSION"
```

- [ ] **Step 5: Fix `docs.yml`'s `build` job**

Find:
```yaml
  build:
    needs: verify-version
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: ./.github/actions/setup-pio
      - run: pio run -e seeed_xiao_esp32c6
```
Replace with:
```yaml
  build:
    needs: verify-version
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: ./.github/actions/setup-pio
      - run: PLATFORMIO_SRC_DIR=examples/BenchTest pio run -e seeed_xiao_esp32c6
```

- [ ] **Step 6: Commit**

```bash
git add .github/workflows/ci.yml .github/workflows/docs.yml
git commit -m "Update CI for the flattened library layout, add arduino-lint job"
```

(These workflows only run in GitHub Actions — there is no local equivalent to "run" them here beyond reading them carefully and re-checking the `PLATFORMIO_SRC_DIR` commands they now call, which Task 4 already verified locally.)

---

### Task 6: Update `Doxyfile` INPUT paths

**Files:**
- Modify: `Doxyfile`

**Interfaces:**
- Consumes: the flattened file locations from Task 1
- Produces: `doxygen/warnings.txt` staying empty (the hard invariant `docs.yml` enforces)

- [ ] **Step 1: Update the header paths in `INPUT`**

Find:
```
                         lib/KonnextraKNX/src/KonnextraKNX.h \
                         lib/KnxObject/src/KnxObject.h \
                         lib/KnxObject/src/KnxLighting.h \
                         lib/KnxObject/src/KnxCovers.h \
                         lib/KnxObject/src/KnxClimate.h \
                         lib/KnxObject/src/KnxDateTime.h \
                         lib/KnxObject/src/KnxScalars.h \
                         lib/KnxValue/src/KnxValue.h \
                         lib/KnxCoordinator/src/KnxCoordinator.h \
                         lib/KnxCommon/src/KnxEnums.h
```
Replace with:
```
                         src/KonnextraKNX.h \
                         src/KnxObject.h \
                         src/KnxLighting.h \
                         src/KnxCovers.h \
                         src/KnxClimate.h \
                         src/KnxDateTime.h \
                         src/KnxScalars.h \
                         src/KnxValue.h \
                         src/KnxCoordinator.h \
                         src/KnxEnums.h
```

- [ ] **Step 2: Regenerate and check for warnings**

```bash
export PATH="/opt/homebrew/bin:$PATH"
rm -rf doxygen
doxygen Doxyfile
cat doxygen/warnings.txt
```
Expected: `doxygen/warnings.txt` is empty. If it isn't, stop — per `CLAUDE.md`, a non-empty `warnings.txt` fails CI, and the fix must happen before this task is done, not deferred.

- [ ] **Step 3: Commit**

```bash
git add Doxyfile
git commit -m "Point Doxyfile at the flattened src/ paths"
```

---

### Task 7: Update `scripts/bump_version.py` for the single `library.properties`

**Files:**
- Modify: `scripts/bump_version.py`

**Interfaces:**
- Consumes: `sys.argv[1]` (new version string), `ROOT / "library.properties"`
- Produces: `VERSION` and `library.properties`'s `version=` line updated together; the release instructions it prints reflect the new file list

- [ ] **Step 1: Replace the `library.json`-globbing logic with a single `library.properties` edit**

Find:
```python
    library_jsons = sorted(ROOT.glob("lib/*/library.json"))
    if not library_jsons:
        print("error: no lib/*/library.json files found", file=sys.stderr)
        sys.exit(1)

    VERSION_FILE.write_text(new_version + "\n")
    print(f"wrote VERSION ({new_version})")

    for path in library_jsons:
        data = json.loads(path.read_text())
        data["version"] = new_version
        path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")
        print(f"updated {path.relative_to(ROOT)}")
```
Replace with:
```python
    lib_properties = ROOT / "library.properties"
    if not lib_properties.exists():
        print("error: library.properties not found at repo root", file=sys.stderr)
        sys.exit(1)

    VERSION_FILE.write_text(new_version + "\n")
    print(f"wrote VERSION ({new_version})")

    text = lib_properties.read_text()
    new_text, count = re.subn(
        r"^version=.*$", f"version={new_version}", text, count=1, flags=re.MULTILINE
    )
    if count != 1:
        print("error: no 'version=' line found in library.properties", file=sys.stderr)
        sys.exit(1)
    lib_properties.write_text(new_text)
    print("updated library.properties")
```

- [ ] **Step 2: Update the trailing `git add` instructions it prints**

Find:
```python
    rel_paths = " ".join(str(p.relative_to(ROOT)) for p in library_jsons)
    print(f"\nVersion set to {new_version}. Next steps (not run automatically):")
    print(f"  git add VERSION {rel_paths} docs/ReleaseNotes.md Changes.md")
```
Replace with:
```python
    print(f"\nVersion set to {new_version}. Next steps (not run automatically):")
    print("  git add VERSION library.properties docs/ReleaseNotes.md Changes.md")
```

- [ ] **Step 3: Remove the now-unused `json` import**

Find (near the top of the file):
```python
import json
import re
```
Replace with:
```python
import re
```

- [ ] **Step 4: Dry-run the script against the current version to confirm it's a no-op**

Run: `python3 scripts/bump_version.py 0.1.7 && git diff --stat`
Expected: prints `wrote VERSION (0.1.7)` and `updated library.properties`; `git diff --stat` shows no content change (version was already `0.1.7` in both files) — if it shows an unexpected diff elsewhere, stop and investigate before committing.

- [ ] **Step 5: Commit**

```bash
git add scripts/bump_version.py
git commit -m "Point bump_version.py at the single root library.properties"
```

---

### Task 8: Update `CLAUDE.md`

**Files:**
- Modify: `CLAUDE.md`

**Interfaces:**
- Consumes: every path/structure fact changed by Tasks 1–7
- Produces: `CLAUDE.md` describing the repo as it now actually is — this task has no code to run, only text to get right; "verification" here is a careful re-read against the new tree, not a command

- [ ] **Step 1: Update the "Repository layout" table**

The `lib/` row and the `examples/` row's build note need to change. Find:
```
| `lib/` | all custom library code (see Architecture below) |
```
Replace with:
```
| `src/` | the library's own source — flat, one file per class/module (see Architecture below) |
```
And in the `examples/` row, add a note that `BenchTest` is the one folder not mirrored in `docs/Examples.md` (it's a hardware bench sketch, not a public API-showcase example):
Find:
```
| `examples/` | standalone `.ino` sketches, one folder each (`DeviceObject`, `StatelessSend`, `CustomKnxObject`). They mirror `docs/Examples.md` — change one, change the other. Not in the build, so **nothing compiles them**; check them by hand after an API change. |
```
Replace with:
```
| `examples/` | standalone `.ino` sketches, one folder each (`DeviceObject`, `StatelessSend`, `CustomKnxObject`, `ExplicitPort`). They mirror `docs/Examples.md` — change one, change the other. `BenchTest` is the exception: it's the hardware bench-test sketch (formerly root `src/main.cpp`), not part of the public showcase, and has no `docs/Examples.md` entry. The showcase sketches are compile-checked by CI's `portability` job (`PLATFORMIO_SRC_DIR=<folder> pio run -e <env>`); check them by hand after an API change beyond that. |
```

- [ ] **Step 2: Rewrite the "Architecture" section's dependency tree and prose**

Find the fenced diagram block starting with:
```
src/main.cpp        ← showcase sketch: wires the stack + drives intent objects
lib/KonnextraKNX/   ← THE public surface, and nothing else: KonnextraKNX.h — the single user
```
and the paragraphs through `No global singletons. Dependencies are injected by constructor pointer/reference.` and the `**User include & construction:**` paragraph after it (all references to `lib/KonnextraKNX/`, `lib/KnxObject/`, `lib/KnxCoordinator/`, etc. as separate libraries).

Replace the whole block with:
```
examples/BenchTest/ ← hardware bench sketch: wires the stack + drives two device objects
src/                ← THE library, flat — everything below is one PlatformIO/Arduino library,
                      one file per class or module, no per-file library.json:
  KonnextraKNX.h    ← the single user include, sole root of the DAG. Defines the KonnextraKNX
                      node class (owns a KnxDriver, built from the physical address).
  Knx{Light,DimmLight,RGB,Blind,Temperature,Humidity,Time,Date,DateTime,Percent,Char,Float}
                    ← KnxObject : IKnxReceiver + intent classes, grouped by domain file
                      (KnxLighting.h, KnxCovers.h, KnxClimate.h, KnxDateTime.h, KnxScalars.h)
  KnxCoordinator.h  ← DI core class KnxCoordinator: group send(ga, KnxValue) + intrusive
                      IKnxReceiver registry, point-to-point sendIndividual/sendControl + one
                      optional IKnxDeviceHandler, loop(); injected IKnxDriver*
  KnxDriver.h/.cpp  ← concrete ATTiny/TP-UART UART driver : IKnxDriver (target-only)
  KnxFrame.h/.cpp, KnxReassembler.h/.cpp
                    ← stateless L_Data framing + reassembler; Arduino-free, host-tested
  KnxValue.h, KnxCodec.h/.cpp
                    ← value currency: KnxValue tagged union + symmetric KnxCodec. Pure
  KnxEnums.h, KnxAddress.h, KnxTelegramTypes.h, KnxInterfaces.h, KnxDebug.h
                    ← shared types + contracts (IKnxDriver / IKnxReceiver / IKnxDeviceHandler,
                      runtime logging switch used by every layer)
examples/           ← standalone .ino sketches mirroring docs/Examples.md (plus BenchTest,
                      which doesn't); NOT in the build, so nothing compiles them automatically
                      except CI's portability matrix
```
Followed by:
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
that pulls in the driver, and nothing includes it — native tests (which include
`KnxCoordinator.h` and the object headers directly) still never drag the Arduino driver into a
host build, because `pio test -e native` only needs `test_build_src = true` to see `src/` at
all, and the test files themselves choose which headers they include.

No global singletons. Dependencies are injected by constructor pointer/reference.
```

Then update the `**User include & construction:**` paragraph — find:
```
**User include & construction:** a sketch needs only `#include <KonnextraKNX.h>` and
`KonnextraKNX knx("1.1.5");`. `KonnextraKNX.h` (its own library, sole root of the DAG) pulls in the
driver, the coordinator core, the value currency, and every intent class, then defines the
user-facing **`KonnextraKNX` node class** — a thin
Arduino subclass of `KnxCoordinator` that *owns* a `KnxDriver` and is built from the physical
address, so the user never instantiates or injects a driver (address typed once). The
dependency-injection **core is `KnxCoordinator`** (`KnxCoordinator.h`): Arduino-free, host-testable
with a mock driver, and the type intent objects reference (`KnxCoordinator&`). Advanced users can
inject their own `IKnxDriver` by constructing a `KnxCoordinator` directly.
```
Replace `KonnextraKNX.h` (its own library, sole root of the DAG)` with `KonnextraKNX.h` (root of the
DAG within the flattened `src/`)` and leave the rest of the paragraph as-is (it describes runtime
behavior, which hasn't changed).

- [ ] **Step 3: Update the "Build system" section**

Find:
```
```
pio run              # build
pio run --target upload
pio device monitor   # 115200 baud
```

All custom code lives under `lib/`. Third-party dependencies are declared in `platformio.ini` and fetched automatically.
```
Replace with:
```
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
```

- [ ] **Step 4: Update the "Testing" line at the end of the Architecture section**

Find:
```
**Testing:** `pio test -e native` runs the host Unity suite (codec, framing, reassembler,
coordinator, objects) against the Arduino-free layers; `pio run` builds the firmware.
```
Replace with:
```
**Testing:** `pio test -e native` runs the host Unity suite (codec, framing, reassembler,
coordinator, objects) against the Arduino-free layers (needs `test_build_src = true` in
`platformio.ini`'s `[env:native]`, since `src/` is no longer pulled in automatically the way
per-folder `lib/` libraries used to be); `PLATFORMIO_SRC_DIR=examples/BenchTest pio run` builds
the firmware.
```

- [ ] **Step 5: Update every other `lib/Knx…` or `lib/KonnextraKNX` path reference in the file**

Run: `grep -n "lib/Knx\|lib/Konnextra" CLAUDE.md`

Expected remaining hits are in the "Bench-test sketch" section header/prose (`src/main.cpp` → `examples/BenchTest/BenchTest.ino`) and the "Debug mode" section's `Implementation is \`KnxCommon/src/KnxDebug.h\`` line → `Implementation is \`src/KnxDebug.h\``. Update each one found:
- Section heading `## Bench-test sketch (\`src/main.cpp\`)` → `## Bench-test sketch (\`examples/BenchTest/BenchTest.ino\`)`
- Every body reference to `src/main.cpp` in that section → `examples/BenchTest/BenchTest.ino`
- `KnxCommon/src/KnxDebug.h` → `src/KnxDebug.h`
- The "Documentation and releases" section's `Doxyfile` bullet list already only names `PREDEFINED`/`RECURSIVE`/`EXCLUDE_SYMBOLS` settings, not paths — leave as-is.
- The release bash snippet's `git add VERSION lib/*/library.json docs/ReleaseNotes.md Changes.md` → `git add VERSION library.properties docs/ReleaseNotes.md Changes.md` (matches Task 7).
- `bump_version.py`'s description `writes VERSION + all 7 library.json files` → `writes VERSION + library.properties`.

- [ ] **Step 6: Final grep sweep — nothing stale left**

Run: `grep -n "lib/Knx\|lib/Konnextra\|7 \`lib/\*/library.json\`\|all 7 library.json" CLAUDE.md`
Expected: no output.

- [ ] **Step 7: Commit**

```bash
git add CLAUDE.md
git commit -m "Update CLAUDE.md for the flattened src/ + examples/BenchTest layout"
```

---

### Task 9: Full verification pass

**Files:** none (verification only)

- [ ] **Step 1: Native tests**

Run: `pio test -e native`
Expected: PASS, all 5 test suites (`test_frame`, `test_coordinator`, `test_codec`, `test_object`, `test_reassembler`).

- [ ] **Step 2: Firmware build**

Run: `PLATFORMIO_SRC_DIR=examples/BenchTest pio run -e seeed_xiao_esp32c6`
Expected: PASS.

- [ ] **Step 3: Every example still compiles** (mirrors what the `portability` CI job now does, including the new `BenchTest` entry)

```bash
for ex in examples/*/; do
  echo "=== $ex ==="
  PLATFORMIO_SRC_DIR="$ex" pio run -e esp32-s3-devkitc-1 || echo "FAILED: $ex"
done
```
Expected: no `FAILED` lines. If `BenchTest` fails here specifically, that's new information the plan didn't anticipate — stop and report it rather than silently excluding it from `examples/`.

- [ ] **Step 4: Doxygen, zero warnings**

```bash
export PATH="/opt/homebrew/bin:$PATH"
rm -rf doxygen
doxygen Doxyfile
cat doxygen/warnings.txt
python3 -m http.server 8080 --directory doxygen/html &
```
Expected: `warnings.txt` empty. Open `http://localhost:8080` and spot-check that `KonnextraKNX`, `KnxLight`, `KnxCoordinator` pages render with content (not blank — a silent `#ifdef` failure earlier would show up here as a vanished class, same trap `CLAUDE.md` already documents for `PREDEFINED`). Kill the server after.

- [ ] **Step 5: `arduino-lint`, if installed locally**

```bash
which arduino-lint && arduino-lint --library-manager update --compliance strict || echo "arduino-lint not installed locally — the new CI job (Task 5, Step 3) will run it in GitHub Actions"
```
Expected: either a clean lint report, or the fallback message (not a hard requirement to have it locally — the CI job is the enforcement point).

- [ ] **Step 6: Confirm `git status` is clean and every change is committed**

Run: `git status && git log --oneline -10`
Expected: clean tree; 9 commits (one per task) since the branch point, in the order Tasks 1–8 were done (Task 9 has no commit of its own — it's verification-only).
