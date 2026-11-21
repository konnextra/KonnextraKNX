# 0002 — Multi-Session-Layout unter `.agents/` statt `docs/`

Datum: 22.11.2026 (Entscheidung Florian).

## Kontext
Die globale CLAUDE.md sieht für Multi-Session-Projekte `docs/START_HERE.md`, `docs/STATUS.md`,
`docs/decisions/`, `docs/specs/`, `docs/context/`, `docs/measurements/` vor. In diesem Repo ist
`docs/` nutzerseitig (`docs/ApiReference.md` ist die Startseite der API-Referenz), und die
Projekt-CLAUDE.md legt Specs bereits unter `.agents/specs/` ab.

## Entscheidung
Gleiche Struktur, aber unter `.agents/`:
`.agents/START_HERE.md`, `.agents/STATUS.md`, `.agents/decisions/`, `.agents/specs/`,
`.agents/context/`, `.agents/bench/` (statt `measurements/`), `.agents/handoff/` (Altbestand).
Die Projekt-CLAUDE.md verweist ganz oben darauf.

## Konsequenzen
- Jede neue Session liest zuerst `.agents/START_HERE.md` und `.agents/STATUS.md`.
- Handoffs gehen in `.agents/STATUS.md`, nicht nur in den Chat.
- `PLAN.md` und `Changes.md` bleiben im Root (Release-Ablauf hängt an `Changes.md`).
