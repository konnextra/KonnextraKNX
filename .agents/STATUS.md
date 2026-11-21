# STATUS

Lebender Stand. Am Ende jeder Session aktualisieren. Letzte Aktualisierung: **22.11.2026**.

## First in the next session

Von Florian gesetzt. Vor allem anderen abarbeiten, erledigte Punkte hier entfernen.

1. **Hardwaretest NCN5130, Test 1: Licht schalten.** Florian lötet am 22.11. die Busklemme an,
   danach ist der Testkoffer bereit. Ablauf und erwartete Log-Zeilen:
   [`context/test-setup.md` → Test 1](context/test-setup.md#test-1-licht-schalten).
   Code liegt auf Branch **`feature/ncn5130`** (Commit `6fb24da`), lokal, **nicht gepusht**.
   Schlägt etwas fehl: Rule 0 — Log lesen, erklären, erst dann ändern. Die ungeprüften Annahmen
   aus dem Datenblatt stehen in [`context/ncn5130.md`](context/ncn5130.md#ungeprüft).
2. **Test 2: zweites Board + Taster + xknx/ETS-Diagnose.** Dafür von Florian einholen: IP der
   IP-Schnittstelle im Testkoffer, statische IP für den Ethernet-Adapter am Mac, GA und
   physikalische Adresse des Tasters. Dann xknx-Mithörer aufsetzen (Homebrew-Python 3.12).
   Testmatrix H1–H14 in der Spec, Abschnitt 5.6.

## Stand

### Fertig
- Release 0.1.9 im Arduino Library Manager + PlatformIO Registry; 0.1.8 entfernt; arduino-lint
  im `update`-Modus; README auf Library-Manager-Installation. Handoff für den Blogpost:
  `.agents/handoff/library-release-0.1.9.md`.
- Datenblattvergleich NCN5130 vs. TP-UART2 → [`context/ncn5130.md`](context/ncn5130.md).
- Konzept: [`specs/2026-11-21-ncn5130-dual-bcu-design.md`](specs/2026-11-21-ncn5130-dual-bcu-design.md).
- **Umsetzung (21./22.11., `feature/ncn5130`, `6fb24da`):**
  - `KnxTransceiver { Ncn5130, TpUart2 }` in `KnxEnums.h`, Standard Ncn5130, letzter
    Parameter in allen `KonnextraKNX`- und `KnxDriver`-Konstruktoren; `transceiver()`-Getter.
  - `KnxTransceiverProtocol` (zustandslos, Arduino-frei): alle Service-Bytes, Klassifizierer,
    SystemStatus-Decoder.
  - `KnxBringUp` (Arduino-frei): Reset → [SystemStatus] → SetAddress → [AwaitConfigure] → State;
    `[ ]` nur NCN5130. Re-poll des Systemstatus, solange der Chip in Sync ist.
  - `KnxDriver` pumpt nur noch `KnxBringUp`; Sende-/Empfangspfad unverändert.
    `KnxReassembler::inFrame()` neu.
  - Tests: `test_transceiver` (9), `test_bringup` (11); alle 84 Host-Tests grün.
  - Builds geprüft: BenchTest auf XIAO C6 und Mega; Uno lehnt den Adress-only-Konstruktor
    weiter ab; Doxygen 0 Warnungen.
  - BenchTest: `#define KNX_CHIP`. ExplicitPort: auskommentierte TP-UART2-Zeile.
  - `Changes.md`: Breaking-Eintrag (Standard-Chip geändert) → Release wird **0.2.0**.

### Offen
- Hardwaretests (siehe oben). Danach Zeitlimits in `KnxTransceiverProtocol` /
  `KnxBringUp` anhand echter Messungen bestätigen oder anpassen.
- Testwerkzeuge `extras/bench/` (BenchPair-Sketch, BusMonitor-Sketch, `knx_listen.py`,
  `run_pair.py`) — Spec 5.3–5.5. Noch nicht geschrieben.
- Doku nach bestandenem Hardwaretest: README, `library.properties`-Absatz, CLAUDE.md
  (Abschnitt Physical layer, Pin-Map-Header), Website en+de (Getting Started, Examples →
  ExplicitPort-Zeile, Supported Boards/Hardware), Forum-Update.
- `PLAN.md` ist teilweise veraltet (nennt `v0.1.7`); bei Gelegenheit auf den Stand bringen.
- Die zwei Defekte aus `PLAN.md` (Read-Request als 0 dekodiert; tote Adressformat-Guards).

### Nächste Schritte
1. Test 1 → Test 2 → H-Matrix.
2. Fixes aus den Tests, dann `extras/bench/`.
3. Doku, Merge nach `main`, Release 0.2.0 (Ablauf in CLAUDE.md).

### Idee, noch nicht dran
Florian möchte die Website-Session und diese Library-Session später bündeln, damit immer im
Gesamtkontext gearbeitet wird. Jetzt nicht umsetzen; nur im Hinterkopf behalten, wenn Doku
für beide Repos anfällt.

## Handoff der letzten Session (21./22.11.2026)

- **Erledigt:** Implementierung wie oben; `.agents/`-Struktur (START_HERE, STATUS, decisions,
  context) angelegt und in CLAUDE.md verlinkt.
- **In Arbeit:** nichts halb Fertiges im Code.
- **Unberührt:** `extras/bench/`, Doku, Website.
- **Blocker:** Busklemme noch nicht angelötet (Florian, 22.11.).
- **Offene Fragen:** Test-2-Angaben (IP-Adressen, Taster-GA) — siehe oben.
- **Dateien:** `src/KnxTransceiverProtocol.{h,cpp}`, `src/KnxBringUp.{h,cpp}`,
  `src/KnxDriver.{h,cpp}`, `src/KonnextraKNX.h`, `src/KnxEnums.h`, `src/KnxReassembler.{h,cpp}`,
  `test/test_transceiver/`, `test/test_bringup/`, `examples/BenchTest/`, `examples/ExplicitPort/`,
  `keywords.txt`, `Changes.md`, `.agents/*`, `CLAUDE.md`.
- **Budget-Hinweis:** Florian achtet auf das Wochenlimit der Nutzung; er nennt pro Tag eine
  Obergrenze. Sparsam arbeiten (keine unnötigen Subagents/Workflows).
