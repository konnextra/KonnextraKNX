# Kontext: Zusammenarbeit mit Florian

Gesammelte Arbeitsweise aus früheren Sessions. Die globale `~/.claude/CLAUDE.md` gilt immer
zusätzlich (Rule 0, Chesterton's Fence, One-way doors, Handoff, Commit-Regeln).

## Kommunikation
- Chat auf **Deutsch**, direkt und knapp. Code, Kommentare und CLAUDE.md auf Englisch.
- Florian diktiert oft per Sprache: „DBUART2“ = TP-UART2, „IT-Klassen“ = die Klassen usw.
- Er nennt ein **Tagesbudget für die Wochennutzung** (z.B. „max. 87 %“) — sparsam arbeiten,
  keine unnötigen Subagents, Workflows oder breiten Recherchen.
- „Keine Rückfragen mehr“ ernst nehmen: mit vernünftigen Defaults durcharbeiten, Annahmen
  am Ende nennen.

## Freigaben
- Nach einmaliger klarer Erklärung eines folgenreichen Schritts (z.B. Tag-Push = Live-Deploy)
  deckt ein knappes „ja/mach“ die **ganze** beschriebene Abfolge ab. Neu erklären nur, wenn sich
  die Art des Schritts ändert (z.B. Force-Push).
- Pushen nur, wenn er es sagt. Entwicklungs-Branches erst anlegen, wenn die Entwicklung startet.

## Git
- Commit-Messages: nur Betreffzeile, kurz, **keine** Claude/AI-Attribution (kein Co-Authored-By).

## Code-Stil (Projekt-Overrides zur globalen style_cpp.md)
1. Methoden **camelCase** (Arduino-Konvention), nicht PascalCase.
2. `#define` statt `static constexpr` wird akzeptiert/bevorzugt (nicht zum Umbau vorschlagen).
3. Bool-Flags `_camelCase`.
4. Kein `#ifdef _MSC_VER` / `#pragma region` in dieser Library.
5. Portabilität: `<stdint.h>` statt `<cstdint>`, kein `Serial.printf` in Sketches/Doku, keine
   Adresse von `static constexpr`-Membern nehmen (AVR, C++11) — siehe CLAUDE.md.
- `check-style`-Ergebnisse als **Tabelle** (Datei, Zeile, Regel, Ist vs. Soll, Schwere).

## Agents
- „Mit ruflo-Agents“ heißt: Agent-Tool mit `ruflo-*`-subagent_type, nicht die Swarm-MCP-Tools.
