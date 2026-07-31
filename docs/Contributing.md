# Contributing {#contributing}

Bug reports and pull requests are welcome. This page is what you need to know before opening
one.

## Reporting a problem

The most useful report is a run with tracing on:

```cpp
knx.enableDebugMode(true);   // before begin()
```

Paste the `[knx]` output along with your board, the sketch, and what you expected. @ref
troubleshooting explains what the lines mean and will often answer the question before you
open an issue.

If the library fails to *build*, the board and the exact compiler error are enough.

## Building and testing

The project is PlatformIO. Two commands cover almost everything:

```bash
pio test -e native      # host unit tests: codec, framing, reassembler, coordinator, objects
pio run                 # build the firmware for the reference board
```

The tests run on your computer, not on hardware, because everything below the Arduino-facing
layer is free of Arduino headers. New protocol logic should come with a test — that is the
only part of the library that can be checked without a bus.

To compile-check a change against another core, point PlatformIO at an example and pick an
environment from `platformio.ini`:

```bash
PLATFORMIO_SRC_DIR=examples/DeviceObject pio run -e megaatmega2560
```

## What CI checks

Every push runs the host tests, the firmware build, and — the one that catches most
regressions — a build of **every example against one board per Arduino core family**. There
is also a job that asserts the address-only constructor *fails* on the Uno, because a board
with no spare serial port must not silently accept it.

Two portability traps are behind that matrix, and both will bite again:

- **AVR has no C++-style C headers and defaults to C++11.** Use `<stdint.h>` and `<string.h>`,
  never `<cstdint>` or `<cstring>`. Do not take the address of a `static constexpr` member —
  copy it to a local first.
- **Do not test the architecture to decide whether `Serial1` exists.** STM32duino declares it
  whenever the chip has a USART1 but only defines it when the sketch asks for it, so an
  architecture check compiles and then fails at link.

## Code style

Follow what is already in the file you are editing. The essentials:

- Tabs. Opening brace on the same line.
- Classes `PascalCase`, methods and members `camelCase`, constants `SCREAMING_SNAKE_CASE`,
  pointer members prefixed `p_`.
- `void` in empty parameter lists: `begin(void)`.
- **No dynamic allocation.** No `new`, no `malloc`, no `std::vector`, no `std::function` —
  this has to run on 8-bit boards. Dependencies are injected through constructors, and there
  are no global singletons. The one deliberate exception is the library-wide debug flag,
  which is a log level rather than a service.
- Doxygen comments on public methods in the header only, written for users: "true if the bus
  confirmed the send", not the name of the protocol field behind it.
- Debug output goes through `KnxDebug::log()`, never `Serial.print`.

## Documentation

Doxygen generates the site from `docs/*.md` plus the public headers. Three rules matter:

- **Zero warnings is enforced.** The docs build fails if `doxygen/warnings.txt` is not empty.
- **A new page must be added to the `Doxyfile` `INPUT` list by hand.** Nothing is picked up
  recursively, and the `INPUT` order is the navigation order.
- **Delete the output directory before regenerating.** Doxygen overwrites but never removes,
  so a stale local build keeps showing pages that no longer exist.

```bash
rm -rf doxygen && doxygen Doxyfile && cat doxygen/warnings.txt
```

If you add a `#ifdef` around anything public, add its macro to `PREDEFINED` in the `Doxyfile`.
Doxygen runs its own preprocessor, and API it cannot resolve simply vanishes from the output
without a warning.

The sketches in `examples/` mirror @ref examples. Change one, change the other — nothing
enforces it, and nothing compiles the `.ino` files.

## Releases

Publishing is tag-driven and deliberately a manual step: the tag push *is* the deploy. The
version lives in `VERSION` and in each library's `library.json`, and CI refuses to publish if
the tag disagrees with any of them.

```bash
python3 scripts/bump_version.py 0.1.7
git add VERSION lib/*/library.json
git commit -m "Bump version to 0.1.7"
git tag v0.1.7
git push && git push --tags
```
