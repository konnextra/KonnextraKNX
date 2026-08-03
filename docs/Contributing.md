# Contributing {#contributing}

Bug reports and pull requests are welcome. This is a small library, so this page is short.

## Reporting a problem

The most useful report is a run with tracing on:

```cpp
knx.enableDebugMode(true);   // before begin()
```

Paste the `[knx]` output along with your board, the sketch, and what you expected.
@ref troubleshooting will often answer the question before you open an issue. If the library
fails to *build*, the board and the exact compiler error are enough.

## Building and testing

```bash
pio test -e native      # host unit tests, no hardware needed
pio run                 # build the firmware for the reference board
```

New protocol logic should come with a test. That is the only part of the library that can be
checked without a bus.

To compile-check against another core, point PlatformIO at an example and pick an environment
from `platformio.ini`:

```bash
PLATFORMIO_SRC_DIR=examples/DeviceObject pio run -e megaatmega2560
```

CI runs the tests, the firmware build, and a build of every example against one board per
Arduino core family. That last one catches most regressions.

## Two traps worth knowing

Both cost real hours to find, and both will bite again.

**AVR has no C++-style C headers and defaults to C++11.** Use `<stdint.h>` and `<string.h>`,
never `<cstdint>` or `<cstring>`. Do not take the address of a `static constexpr` member, copy
it to a local first.

**Do not test the architecture to decide whether `Serial1` exists.** STM32duino declares it
whenever the chip has a USART1 but only defines it when the sketch asks for it, so an
architecture check compiles and then fails at link.

## Style

Match the file you are editing. The one rule that is not negotiable: **no dynamic allocation**.
No `new`, no `malloc`, no `std::vector`, no `std::function`. This has to run on 8-bit boards.

Public methods get a Doxygen comment in the header, written for users. Debug output goes through
`KnxDebug::log()`, never `Serial.print`.

## Documentation

```bash
rm -rf doxygen && doxygen Doxyfile && cat doxygen/warnings.txt
```

- **Zero warnings is enforced.** The docs build fails if `warnings.txt` is not empty.
- **A new page must be added to the `Doxyfile` `INPUT` list by hand.** Nothing is picked up
  recursively, and the `INPUT` order is the navigation order.
- **Delete the output directory before regenerating.** Doxygen overwrites but never removes, so
  a stale build keeps showing pages that no longer exist.
- If you add an `#ifdef` around anything public, add its macro to `PREDEFINED`. Doxygen runs its
  own preprocessor, and API it cannot resolve vanishes from the output without a warning.

The sketches in `examples/` mirror @ref examples. Change one, change the other, nothing enforces
it.

## Releases

The version lives in `VERSION` and in each library's `library.json`. CI refuses to publish if
they disagree. Pushing the tag is what deploys.

A change that a user would notice gets a line in `Changes.md` at the root, at the time you make
it. Those lines are rewritten into @ref releasenotes when a version is cut, and `Changes.md`
starts empty again.

```bash
python3 scripts/bump_version.py 0.1.7
git add VERSION lib/*/library.json docs/ReleaseNotes.md Changes.md
git commit -m "Bump version to 0.1.7"
git tag v0.1.7
git push && git push --tags
```

## Licence

The library is published under the **BSD 3-Clause Licence**, the full text of which is in
`LICENSE` at the root of the repository. In short: use it, change it, ship it inside a
commercial product. Keep the copyright notice, and do not use the Konnextra name to advertise
something you built from it.

BSD 3-Clause says nothing about contributions, so this page does. **Opening a pull request means
your contribution is offered under those same terms.** There is nothing else to sign.
