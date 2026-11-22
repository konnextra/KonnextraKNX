# STATUS

Lebender Stand. Am Ende jeder Session aktualisieren. Letzte Aktualisierung: **22.11.2026 (abends)**.

## First in the next session

Von Florian gesetzt. Vor allem anderen abarbeiten, erledigte Punkte hier entfernen.

1. **Test 2: Empfang + Mitschnitt.** Test 1 (Senden) ist bestanden →
   [`bench/2026-11-22-ncn5130-test1.md`](bench/2026-11-22-ncn5130-test1.md). Florian hängt den
   Mac ins WLAN der IP-Schnittstelle (kein Internet, daher ohne Claude) und lässt
   `extras/bench/knx_listen.py` mitschneiden, während der BenchTest läuft und er am Taster
   schaltet; parallel den seriellen Log des XIAO speichern. Danach beide Dateien auswerten:
   jedes TX des XIAO muss im xknx-Log erscheinen, jedes Taster-Telegramm im XIAO-Log
   (`RX`/`0 receiver(s)` bei fremden GAs). Ablauf: [`context/test-setup.md`](context/test-setup.md#test-2-empfang--mitschnitt).
   Danach zweites Board (1.1.6), Testmatrix H1–H14 (Spec 5.6).
2. **Entscheidung RESETB/SAVEB** (Vorschlag unten unter „Vorschläge“) mit Florian klären.

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

### Vorschläge (keine Entscheidungen)
**RESETB / SAVEB nutzen (22.11.).** Beim NCN5130 sind beide **Ausgänge des Chips**
(Open-Drain, interner Pull-up auf VDDD 3,3 V, Header-Pins 7/8) — der Host kann damit den Chip
nicht zurücksetzen, nur seinen Zustand lesen. RESETB low = Busspannung zu niedrig / DC-DC
nicht gestartet / Thermal Shutdown / Watchdog; **keine UART-Kommunikation möglich**. SAVEB low =
Warnung (VFILT < 14 V sofort, VBUS < 20 V nach 2 ms, Übertemperatur). Wichtige Folge:
Nach einem Spannungseinbruch macht der NCN5130 einen Reset und **verliert die Adresse →
kein Auto-ACK mehr**, ohne dass die Library es merkt.
- A) Optionale Pins: `knx.setStatusPins(resetPin, savePin)` (vor `begin()`); `begin()` wartet,
  bis RESETB high ist; `loop()` erkennt die steigende RESETB-Flanke und ruft `reset()` (neuer
  Bring-up) automatisch; `knx.onBusPowerFail(cb)` bei SAVEB low. Funktioniert auch beim
  TP-UART2 (hat RESET- und SAVE-Ausgänge mit gleicher Bedeutung, gegen Datenblatt prüfen).
- B) Ohne Pins: einen spontanen `U_Reset.ind` (0x03 außerhalb eines Frames) in `poll()`
  erkennen und automatisch neu hochfahren. Kostet keine Pins, erkennt aber keinen Power-Fail
  vorab.
- Empfehlung: **B als Default + A optional** — B schützt jeden Nutzer, A ist für Geräte, die
  bei Busausfall Daten sichern wollen.

### Idee, noch nicht dran
Florian möchte die Website-Session und diese Library-Session später bündeln, damit immer im
Gesamtkontext gearbeitet wird. Jetzt nicht umsetzen; nur im Hinterkopf behalten, wenn Doku
für beide Repos anfällt.

## Handoff der letzten Session (21./22.11.2026)

- **22.11. abends:** Test 1 bestanden (Senden, Bring-up). `extras/bench/knx_listen.py`
  geschrieben (xknx 4, JSONL-Log, Auto-Discovery), Venv `extras/bench/.venv` angelegt (gitignored).
  RESETB/SAVEB-Vorschlag notiert.
- **Erledigt (21./22.11.):** Implementierung wie oben; `.agents/`-Struktur (START_HERE, STATUS, decisions,
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
