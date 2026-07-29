# PLAN — Documentation Phase

The library itself is complete and on `main`. This phase is **documentation only** — no
behaviour changes. Everything is done except **Step 4c**.

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
| 3 | Site-chrome integration — Doxygen output wrapped in the real Innotree navbar/footer | (follow-ups to `23215ad`) | `.agents/specs/2026-07-21-doxygen-site-integration-design.md` |
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

## Open decisions (waiting on the user)

- **`KnxEnums.h` is mixed-scope** — three documented user-facing enums live alongside untouched
  internal ones, currently handled with `EXCLUDE_SYMBOLS` in the `Doxyfile`. Revisit if the file
  grows.
- **Root `README.md`** was moved to `docs/GettingStarted.md` and not replaced. The repo's GitHub
  landing page is empty until a new, shorter `README.md` is written.
