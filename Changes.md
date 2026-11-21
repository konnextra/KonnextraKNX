# Unreleased changes

Working notes for the next release. This file is **not published** — it is not in the `Doxyfile`
`INPUT` list. Before a tag its contents are rewritten into the Release Notes page, which lives in
the website repo (`docs/content/en/releasenotes.md` and `de/releasenotes.md` in
`konnextra/Website_`); afterwards this file is emptied back to the template below.

**What earns a line.** The same bar the Release Notes page sets for itself: a change that makes
someone edit their sketch, behave differently on the bus, or see something new. Refactors, tests,
CI and documentation touch-ups do not belong here.

**Write it now, in a user's words.** That is the entire point of the file. Reconstructed from
`git log` at release time, release notes read like a commit list; written while the change is
fresh, they read like an explanation.

**Prefix anything that breaks an existing sketch with `Breaking:`.** Those are the entries a
reader is actually scanning for.

## Changes

<!-- - Breaking: `begin()` no longer opens the port. Call `setPins()` before it. -->
- Breaking: the library now targets the onsemi **NCN5130** (KONNEXTRA BCU light) by default. On a
  TP-UART2 — including the STKNX/ATtiny bridge — name the chip as the last constructor argument:
  `KonnextraKNX knx("1.1.5", KnxTransceiver::TpUart2);`. Without it, `begin()` returns false on a
  TP-UART2.
- New: `KnxTransceiver` selects the bus coupler chip, in every `KonnextraKNX` constructor.
- `begin()` no longer mistakes bus traffic during start-up for the chip's answers, so it also
  succeeds on a busy bus. With debug mode on, the NCN5130's supply rails and operating mode are
  logged at start-up.
