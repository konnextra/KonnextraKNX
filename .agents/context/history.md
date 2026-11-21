# Kontext: Projektgeschichte

Kurzfassung. Ausführliche Zeitleiste des Library-Manager-Releases:
`.agents/handoff/library-release-0.1.9.md`. As-built-Details: `PLAN.md`, `CLAUDE.md`, git log.

| Wann | Was |
|---|---|
| bis Mitte 2026 | Diplomarbeit: KNX-Wandcontroller (Touch, OLED, RGB) mit STKNX + ATtiny-Bridge, Treiber `KNX_TPUART2`. Der Button-Layer `examples/KNX_Device/` ist entfernt (git history). |
| Juli 2026 | Redesign zur Adafruit-artigen Library (`KnxObject`, Intent-Klassen, `KnxValue`, `KnxCoordinator`, echter `L_Data.con`). Senden auf Hardware verifiziert (0x8B). Doxygen-Site + CI/CD (tag-getriebene Pages). |
| nach 20.07.2026 | Repo/Org umbenannt: jetzt **`konnextra/KonnextraKNX`** (früher `innotree-labs/KNX_Library` — alte Erwähnungen sind veraltet). |
| 21.08.2026 | Flaches `src/`-Layout für den Arduino Library Manager, `library.properties`, arduino-lint. |
| 06./07.10.2026 | v0.1.8, v0.1.9; Registry-PR arduino/library-registry#9305 in einer Minute gemergt. |
| 08./09.10.2026 | Im Index; PlatformIO Registry automatisch; 0.1.8 entfernt (Issue #9315). |
| 18.11.2026 | NCN5130-Platinen (KONNEXTRA BCU light) angekommen. |
| 19.11.2026 | Datenblattvergleich; Entscheidung beide Chips (decisions/0001). |
| 21./22.11.2026 | Konzept + Umsetzung auf `feature/ncn5130`; `.agents/`-Struktur (decisions/0002). |

Noch offene Hardware-Follow-ups aus dem Redesign (nicht host-testbar): Wiederholte Frames auf
belebtem Bus, kollidierender TX, echter Ping per `sendIndividual`, `DimmIncrement`-Nummerierung.
Bewusst zurückgestellt: L_Data_Extended-Frames, Transport-Verbindungsautomat (für ETS-Verbindungen).
