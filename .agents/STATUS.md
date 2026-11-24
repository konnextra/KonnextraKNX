# STATUS

Lebender Stand. Am Ende jeder Session aktualisieren. Letzte Aktualisierung: **24.11.2026**.

## First in the next session

Von Florian gesetzt (22.11. spät). Vor allem anderen abarbeiten, erledigte Punkte entfernen.
Arbeitsstand: Branch **`feature/ncn5130`** (gepusht). Senden **und** Empfangen über den NCN5130
sind auf Hardware bestätigt, auch gegen xknx (57/57) — siehe
[`bench/2026-11-22-ncn5130-test2-rx.md`](bench/2026-11-22-ncn5130-test2-rx.md).

1. **Lasttest mit zwei Boards, Kollisionen erzwingen (Hauptthema).** Zwei Boards am Bus spammen
   sich gegenseitig zu, damit echte Kollisionen und Wiederholungen entstehen; xknx schneidet
   parallel mit (ETS-Gruppen-/Busmonitor optional, für Florian umständlicher).
   - Vorbereitung durch Claude: Lasttest-Sketch (z.B. `extras/bench/BenchPair/`, Spec 5.3):
     Rolle A/B per `#define`, Adressen 1.1.5 / 1.1.6, einstellbare Senderate bis „so schnell wie
     möglich“, startet beide Sender zeitgleich (z.B. gemeinsamer Takt oder Tastendruck), zählt
     gesendet / positiv / negativ / empfangen vom Partner und druckt maschinenlesbare Zeilen.
     Dazu `compare_logs.py` für **zwei** Serial-Logs + xknx erweitern.
   - Auswerten: Verluste, negative `L_Data.con`, Wiederholungen (Steuerbyte `9x`), ob jede
     positiv bestätigte Sendung bei xknx und beim Partner ankommt, Durchsatz in Tel./s.
   - Florian braucht dafür: zweites XIAO + zweite BCU light am Bus, zweiter USB-Port.
2. **Callback auf Hardware zeigen:** Aktor sendet keinen Status auf 1/1/1 oder 0/2/1. Status-GA
   des Aktors aus ETS eintragen, oder testweise `KnxLight lamp(knx, "0/1/1", "0/1/1")` → Taster
   muss `onLampChanged` auslösen.
3. **Status-Pins testen** (decisions/0003, implementiert, ungetestet): BCU-Header 7 RESETb und 8
   SAVEb an zwei freie XIAO-Pins, `KNX_RESET_PIN`/`KNX_SAVE_PIN` im BenchTest setzen, Busspannung
   kurz trennen → erwartet `save pin low` + `[power]`, `chip in reset`, danach `re-running
   bring-up`; Licht schaltet weiter. Dasselbe ohne Pins: kommt `unexpected Reset.ind`?

**Neue Testplatine (23.11., noch nicht fertig gelötet):** Protoboard im Schalterrahmen mit BCU
light (links), XIAO ESP32-C6, 2 grüne LEDs (Status der zwei Testkoffer-Lampen) und 2 Mikrotastern.
Mit Florian festgelegte Belegung — Sketch und Test 3 darauf aufbauen:

| Pin | GPIO | Funktion |
|---|---|---|
| D0 | 0 | RESETB (BCU RS) → `KNX_RESET_PIN` |
| D1 | 1 | SAVEB (BCU SV) → `KNX_SAVE_PIN` |
| D2 | 2 | LED 1 (Lampe 1), 220 Ω gegen GND (47 Ω falls InGaN-Grün ~3 V) |
| D3 | 21 | Taster 1 gegen GND, `INPUT_PULLUP` |
| D4 / D5 | 22 / 23 | I2C SDA / SCL — **frei halten** |
| D6 / D7 | 16 / 17 | KNX TX / RX (`setPins(D7, D6)`) |
| D8 | 19 | Taster 2 gegen GND, `INPUT_PULLUP` |
| D9 | 20 | LED 2 (Lampe 2), 220 Ω gegen GND |
| D10 | 18 | frei (Reserve) |

Erst Status fragen: fertig gelötet? LEDs tatsächlich mit welchem Widerstand bestückt?

**Werkzeuge (alles fertig):** XIAO-Log
`cd ~/Coding/Softwarestack_STKNX/extras/bench && ~/.platformio/penv/bin/pio device monitor -p /dev/cu.usbmodem101 -b 115200 -f time -f log2file`;
xknx `extras/bench/.venv/bin/python extras/bench/knx_listen.py --gateway 192.168.0.10`
(Mac im WLAN `ROWI-Testkoffer-KNX`, Schnittstelle **nicht** Secure);
Abgleich `python3 extras/bench/compare_logs.py <serial.log> <capture.jsonl> --window 1.5`.
Details: [`context/test-setup.md`](context/test-setup.md).

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
- **Zustandsloses Empfangen (`subscribe`/`onTelegram`, 24.11.) — Code fertig, auf
  `feature/ncn5130`, nicht gepusht.** Spec `specs/2026-11-23-subscribe-design.md`, Plan
  `specs/2026-11-24-subscribe-plan.md`, decisions/0004. Offen dazu: Abschnitt „Subscribe“ im
  Examples-Guide der Website (en + de); Hardwaretest (Taster 1.1.1 → 0/1/1 abonnieren);
  Forum-Antwort an LostWolf nach dem Release. Zurückgestellte Kleinigkeiten aus dem Review:
  Null-Check liest inaktives Union-Member; `subscribe()` im Callback sieht das laufende Telegramm
  je nach Slot; Debug-Zähler kann über 255 laufen; `KnxTelegram::payload()` liefert bei Read 1 Byte.
- **Log-Stufen `Off/Basic/Deep` + `setDebugOutputPort(Print*)`** — Spec
  `specs/2026-11-24-log-levels-design.md`, nächster Schritt nach subscribe: Plan schreiben,
  vorher heutige Log-Aufrufe gegen einen echten Bench-Log durchgehen.
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
**Echter Busmonitor-Modus (`U_Busmon`, 24.11. von Florian geparkt):** `knx.beginBusMonitor()`
statt `begin()`, Chip schickt jedes Busbyte roh inkl. ACK/NAK, Gerät passiv (sendet/quittiert
nicht), Telegrammende über Pausen. Ausgabe im selben Format wie die Log-Stufen plus ACK-Zeilen.
Vorher NCN5130- und TP-UART2-Datenblatt zum Busmon-Byte-Strom lesen, eigene Spec. Wird auch für
den Lasttest (ACKs/Wiederholungen sehen) gebraucht. Reihenfolge: subscribe → Log-Stufen → Busmonitor.

Florian möchte die Website-Session und diese Library-Session später bündeln, damit immer im
Gesamtkontext gearbeitet wird. Jetzt nicht umsetzen; nur im Hinterkopf behalten, wenn Doku
für beide Repos anfällt.

## Handoff der Session 23./24.11.2026

- **Erledigt:** subscribe-Feature nach Plan umgesetzt (Commits `1841937`..`006c853`):
  `KnxTelegram` (neu, `src/KnxTelegram.h`), `KnxCodec::decodeTelegram`, `onTelegram`,
  `subscribe`/`unsubscribe`/`setSubscriptionSlotLimit` (einmal `calloc`, Ausnahme laut 0004),
  String-Überladungen, `examples/Subscribe`, README, keywords, Changes.md, Doxyfile. 111
  Host-Tests grün; Subscribe baut auf C6, Mega, R4; BenchTest baut; Doxygen 0 Warnungen.
- **Entschieden:** decisions/0004 (zustandsloses Empfangen, malloc-Ausnahme); Log-Stufen-Design;
  Busmonitor geparkt (siehe „Idee, noch nicht dran“).
- **Nicht getan:** Push, Website, Hardwaretest, Log-Stufen.
- **Dateien:** `src/KnxTelegram.h`, `src/KnxCodec.{h,cpp}`, `src/KnxObject.h`,
  `src/KnxCoordinator.{h,cpp}`, `test/test_telegram/`, `test/test_coordinator/`,
  `examples/Subscribe/`, `README.md`, `keywords.txt`, `Changes.md`, `Doxyfile`, `.agents/*`.

## Handoff der Session davor (21./22.11.2026)

- **22.11. 23:20:** Empfang und Senden gegen xknx abgeglichen, 57/57, kein Verlust unter Last.
  `extras/bench/compare_logs.py` neu.
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
- **Blocker:** keiner. Für Punkt 1 braucht es das zweite Board am Bus.
- **Offene Fragen:** Status-GA des Aktors (für den Callback-Test).
- **Dateien:** `src/KnxTransceiverProtocol.{h,cpp}`, `src/KnxBringUp.{h,cpp}`,
  `src/KnxDriver.{h,cpp}`, `src/KonnextraKNX.h`, `src/KnxEnums.h`, `src/KnxReassembler.{h,cpp}`,
  `test/test_transceiver/`, `test/test_bringup/`, `examples/BenchTest/`, `examples/ExplicitPort/`,
  `keywords.txt`, `Changes.md`, `.agents/*`, `CLAUDE.md`, `.gitignore`,
  `extras/bench/knx_listen.py`, `extras/bench/compare_logs.py`.
- **Budget-Hinweis:** Florian achtet auf das Wochenlimit der Nutzung; er nennt pro Tag eine
  Obergrenze. Sparsam arbeiten (keine unnötigen Subagents/Workflows).
