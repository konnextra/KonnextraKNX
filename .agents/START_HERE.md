# START HERE — KonnextraKNX

Einstieg für jede neue Session. Erst diese Datei, dann [`STATUS.md`](STATUS.md) (oberster
Abschnitt zuerst), dann nur das lesen, was die Aufgabe braucht. Die Repo-Regeln stehen in
[`../CLAUDE.md`](../CLAUDE.md) — diese Datei wiederholt sie nicht.

## Ziel

**KonnextraKNX** ist eine Arduino-Library im Adafruit-Stil, mit der ein Sketch ohne ETS am
KNX-Bus spricht (`#include <KonnextraKNX.h>`, `KonnextraKNX knx("1.1.5");`, Geräteobjekte wie
`KnxLight`). Seit 7.10.2026 im Arduino Library Manager und in der PlatformIO Registry, aktuell
**0.1.9 (erste Beta)**. Lizenz BSD-3-Clause. Firma/Marke: Konnextra (konnextra.at).

Hardware: Seit November 2026 ist die eigene BCU die **KONNEXTRA BCU light** mit onsemi
**NCN5130** (3,3 V UART, *nicht* galvanisch getrennt). Davor STKNX + ATtiny-Bridge
(TP-UART2-Protokoll). Die Library soll **beide** Chips können — das ist die laufende Arbeit.

## Wo was liegt

| Was | Wo |
|---|---|
| Repo-Regeln, Architektur, Build, Release-Ablauf, Pin-Map | `CLAUDE.md` |
| Lebender Stand, nächste Schritte, Handoff | `.agents/STATUS.md` |
| Entscheidungen (eine Datei je Entscheidung) | `.agents/decisions/NNNN_slug.md` |
| Specs / Konzepte je Phase | `.agents/specs/` |
| Hintergrund aus früheren Sessions (Kontext, keine Entscheidung) | `.agents/context/` |
| Messungen, Bench-Logs | `.agents/bench/` (anlegen beim ersten Eintrag) |
| Ältere Handoffs (z.B. für den Blogpost) | `.agents/handoff/` |
| Was noch offen ist (alte Gesamtplanung) | `PLAN.md` |
| Unveröffentlichte Änderungen für die Release Notes | `Changes.md` |
| Library-Code | `src/` (flach) |
| Host-Tests | `test/` (`pio test -e native`) |
| Datenblätter, KNX-Spezifikation, Schaltplan der BCU | `reference/` — **nur lokal, gitignored** |

**Warum `.agents/` und nicht `docs/`:** Die globale CLAUDE.md sieht `docs/STATUS.md` usw. vor.
In diesem Repo ist `docs/` nutzerseitig (Startseite der API-Referenz) und der Library Manager
liefert das Repo aus, darum liegt alles Interne unter `.agents/` (Entscheidung
[0002](decisions/0002_agents-folder-layout.md)).

## Zweites Repo: die Website

Die Nutzer-Guides (Getting Started, Examples, Supported Boards, Release Notes, en + de) liegen
**nicht hier**, sondern in `konnextra/Website_`, lokal unter
`/Applications/XAMPP/xamppfiles/htdocs/Website_` (`docs/content/{en,de}/`). Ändert sich hier
etwas, das ein Guide beschreibt, braucht es dort einen Commit in beiden Sprachen. Die Website
hat eine eigene CLAUDE.md und `.agents/`; hier nichts davon anfassen, ohne gefragt zu werden.

## Wichtigste Erkenntnisse (Kurzfassung)

- **NCN5130 ≠ TP-UART2 beim Adresse-Setzen:** TP-UART2 `0x28 hi lo`, NCN5130 `0xF1 hi lo 0x00`.
  `0x28` auf dem NCN5130 schreibt das Watchdog-Register und führt das Low-Byte als Befehl aus —
  für x.x.5 ist das `0x05` = U_Busmon.req. Details: [`context/ncn5130.md`](context/ncn5130.md).
- **Bring-up als Zustandsautomat** (`KnxBringUp`), Bytes innerhalb eines Busframes zählen nie
  als Chip-Antwort. Begründung: [`decisions/0001`](decisions/0001_dual-bcu-single-driver.md).
- **Senden ist hardware-verifiziert** (TP-UART2/STKNX, positiver `L_Data.con` 0x8B), **Empfang
  noch nicht** — beim bisherigen Test war kein Fremdverkehr am Bus.
- **Board nicht isoliert:** GND = Busminus. Am Bus nie über geerdetes Netzteil/USB arbeiten.
- Testaufbau, Werkzeuge, Pfade: [`context/test-setup.md`](context/test-setup.md).
- Arbeitsweise mit Florian: [`context/working-with-florian.md`](context/working-with-florian.md).
- Projektgeschichte: [`context/history.md`](context/history.md).

## Modelltabelle (aus der globalen CLAUDE.md, hier unverändert)

| Arbeit | Modell |
|---|---|
| Architektur, Specs, Entscheidungen und ihre Known-Answer-Tests | stärkstes verfügbares, hohe Effort |
| Korrektheitskritische Implementierung (Treiber, Bring-up, Frame/Codec) | mindestens Opus |
| Plumbing, IO, CLI, Scaffolding, Doku-Pflege, Website-Texte | Sonnet |
| Lese-, Such-, Zähljobs (Subagents) | Haiku oder Sonnet |

Passt das eigene Modell nicht zur Aufgabe: vor der Arbeit Bescheid geben, mit Empfehlung.
