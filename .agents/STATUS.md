# STATUS

Lebender Stand. Am Ende jeder Session aktualisieren. Letzte Aktualisierung: **22.11.2026 (abends)**.

## First in the next session

Von Florian gesetzt. Vor allem anderen abarbeiten, erledigte Punkte hier entfernen.

1. **Test 2b: Callback + xknx-Gegenprobe.** Empfang von Fremdverkehr ist bestanden →
   [`bench/2026-11-22-ncn5130-test2-rx.md`](bench/2026-11-22-ncn5130-test2-rx.md). Offen:
   - **Callback auf Hardware zeigen:** Aktor sendet keinen Status auf 1/1/1/0/2/1. Entweder
     Status-GA des Aktors aus ETS in BenchTest eintragen, oder testweise
     `KnxLight lamp(knx, "0/1/1", "0/1/1")` → Taster muss `onLampChanged` auslösen.
   - **xknx-Gegenprobe:** Schnittstelle 192.168.0.10 (`IP Interface Secure N 148/23`, 1.1.4).
     `--gateway 192.168.0.10` (ohne spitze Klammern!). Lehnt sie ab → Secure: ETS-Keyring
     exportieren, `--keyring file.knxkeys --keyring-password …`. `--scan` zeigt jetzt
     `tunnelling requires secure`.
   - XIAO-Log mitschreiben: `pio device monitor | tee` geht nicht (braucht TTY) → stattdessen
     `cd extras/bench/logs && ~/.platformio/penv/bin/pio device monitor -b 115200 -f time -f log2file`.
2. **Hardwaretest Status-Pins** (decisions/0003, implementiert, ungetestet): BCU-Header 7 RESETb
   und 8 SAVEb an zwei freie XIAO-Pins, im BenchTest `KNX_RESET_PIN`/`KNX_SAVE_PIN` setzen,
   Busspannung kurz trennen → erwartet: `save pin low` + `[power]`-Zeile, `chip in reset`, nach
   Rückkehr `re-running bring-up` und wieder `begin`-artige ok-Zeile; Licht schaltet weiter.
   Ohne Pins dasselbe einmal probieren: kommt `unexpected Reset.ind`? (prüft Selbstheilung)

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
- Doku nach bestandenem Hardwaretest (inkl. Status-Pins: **TP-UART2-RESn setzt zurück, NCN5130-
  RESETB nur lesen**, 3,3 V Open-Drain, Pegelwandler bei 5 V): README, `library.properties`-Absatz, CLAUDE.md
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

- **22.11. Nacht (2):** Test 2a bestanden — **Empfangspfad auf Hardware bestätigt** (Taster
  1.1.1: Schalten, Dimmen DPT 3, Jalousie). Keyring-Option + Secure-Anzeige im Listener.
- **22.11. Nacht:** Status-Pins auf Interrupt-Latches umgestellt (Reaktion weiter in `loop()`),
  Matrix C6/ESP32/S3/Mega/R4/GIGA/Pico/Pico2/F401/L432 + Uno-Fehlfall ok; `knx_listen.py` um
  `--scan` und `--routing` erweitert.
- **22.11. spät:** Status-Pins + Selbstheilung umgesetzt (decisions/0003): `setStatusPins`,
  `onBusPowerFail`, `busPowerOk` in `KonnextraKNX`/`KnxDriver`, BenchTest-Defines, Changes.md.
  Builds C6/Mega/R4/Nucleo ok, 84 Host-Tests ok, Doxygen 0 Warnungen. Noch nicht auf Hardware.
  Florians Frage: `pio device monitor` flasht nicht, liest nur mit.
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
