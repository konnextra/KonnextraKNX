# Konzept: NCN5130 und TP-UART2 parallel unterstützen

Stand 21.11.2026. Nur Konzept, noch kein Code. Entwicklung später auf einem eigenen Branch
(Vorschlag: `feature/ncn5130`), der erst angelegt wird, wenn die Umsetzung beginnt.

Grundlagen: `reference/NCN5130-D.pdf`, `reference/TP-UART2.pdf`,
`reference/BCU-light-schematic.pdf` (Rev C). Alle drei liegen nur lokal, weil `reference/` per `.gitignore` ausgeschlossen ist.

---

## 1. Context

### Ausgangslage
- Die Library spricht heute ausschließlich das TP-UART2-Protokoll. Verifiziert ist sie gegen das
  STKNX/ATtiny-Board und gegen einen Siemens TP-UART2.
- Die neue Hardware ist die **KONNEXTRA BCU light** mit onsemi **NCN5130**. Sie ist nicht
  galvanisch getrennt, GND liegt auf Bus−, und die UART läuft mit 3,3 V.
- Laut Schaltplan liegen MODE1, MODE2, TREQ, UC1 und UC2 auf GND. Das ergibt **9-Bit-UART
  mit 19200 Baud**, also 8E1. Das ist unsere bestehende Leitungseinstellung, und
  `U_FrameState.ind` tritt nicht auf.

### Anforderung (Florian, 19. und 21.11.)
- Beide BCUs müssen funktionieren („zweischienig“).
- Standard ist der NCN5130. Der TP-UART2 lässt sich im Konstruktor auswählen.
- Die Tests müssen beide Chips abdecken, auch mit zwei Boards gleichzeitig am Bus und mit
  xknx als unabhängigem Mithörer.

### Vergleich der Datenblätter: was den Treiber betrifft

| Thema | TP-UART2 | NCN5130 | Folge für den Treiber |
|---|---|---|---|
| Adresse setzen | `0x28 hi lo` | `0xF1 hi lo dummy` | **Muss je Chip verschieden sein** (siehe unten) |
| `0x28` auf dem anderen Chip | – | `U_IntRegWr` Register 0 (Watchdog), danach wird das letzte Adressbyte als Befehl gelesen | Mit 1.1.5 wird `0x05` gelesen, also `U_Busmon.req`. Der NCN5130 geht in den Busmonitor, und `begin()` schlägt fehl |
| Bestätigung der Adresse | keine | `U_Configure.ind` = `0 b aa ap c m 0 1` (z.B. `0x21`), kommt erst, wenn der Bus frei ist | Darauf warten. Das ist zugleich ein echter Beweis, dass die automatische Bestätigung (Auto-ACK) aktiv ist |
| Auto-ACK | nach `U_SetAddress` alle Gruppentelegramme und die eigene Adresse | identisch | unverändert |
| Dauer des Resets | wartet, bis der Bus frei ist; Host soll 50 ms nach dem Einschalten warten | `tRESET` ≈ 8 ms, dann Power-Up → Sync (40 Tbit Ruhe) → Normal | Zeitlimit für `U_Reset.ind` je Chip, beim NCN5130 großzügig (200 ms) |
| Systemstatus | nicht vorhanden | `U_SystemStat.req 0x0D` → `0x4B` + Byte (V20V, VDD2, VBUS, VFILT, XTAL, TW, Mode) | Nur NCN5130: Diagnose in `begin()`, zeigt im Debug-Log, *warum* das Board nicht bereit ist |
| Reset, Status | `0x01`/`0x03`, `0x02`/`0x07` | identisch | gemeinsam |
| `U_Ackn` / `U_AckInformation` | `0x10–0x17` | identisch | gemeinsam (wird ohnehin nicht genutzt) |
| Senden | `0x80`, `0x80\|i`, `0x40\|l` | identisch | gemeinsam, ebenso unser `0x40 \| (Länge−1)` |
| Echo des eigenen Frames | ja | ja (als `L_Data.ind`) | gemeinsam |
| `L_Data.con` | `0x8B` positiv / `0x0B` negativ | identisch | gemeinsam |
| Wiederholungen | 3 | 3 (`U_SetRepetition 0xF2`) | gemeinsam |
| CRC und Marker | eigene Befehle | `U_Configure 0x18–0x1F` | nicht genutzt |
| Hardware-Reset | – | RESETB-Pin (Header Pin 7), keine UART-Verbindung, solange er low ist | später optional |

**Fazit:** Rund 90 % des Treibers (Port, Senden, Echo, `L_Data.con`, Empfang über den
Reassembler) sind gleich. Verschieden sind nur die **Inbetriebnahme** (Adresse setzen,
Bestätigung abwarten, Zeitlimits, Diagnose) und **ein Befehl**.

Achtung, Rückwärtskompatibilität: Mit NCN5130 als Standard bekommt ein bestehender TP-UART2-Nutzer
nach dem Update `0xF1 …` geschickt. Wie der TP-UART2 darauf reagiert, ist undokumentiert.
Erwartet: Protokollfehler, `begin()` liefert false. Daher wird das Release **0.2.0** und
bekommt einen deutlichen Hinweis in den Release Notes.

---

## 2. Architektur-Optionen

### Option A: ein Treiber, Chip als Enum, Protokoll ausgelagert (**empfohlen**)
`KnxDriver` bleibt **eine** Klasse und bekommt ein `KnxTransceiver`-Member. Alles, was sich je Chip
unterscheidet, wandert in ein neues Modul `KnxTransceiverProtocol` ohne Arduino-Abhängigkeit
(Befehlsbytes, Zeitlimits, Einordnung der Antwortbytes). Der Treiber verzweigt nur in der Inbetriebnahme.
- \+ Der bereits auf Hardware verifizierte Sende- und Empfangspfad bleibt **unangetastet**.
- \+ Kein Heap, kein zusätzliches RAM, keine Templates. Die Nutzer-API bleibt `KonnextraKNX knx("1.1.5");`.
- \+ Das Protokoll wird auf dem Host testbar (bisher ist `KnxDriver.cpp` aus den Host-Tests ausgeschlossen).
- − An ein bis zwei Stellen gibt es einen `switch` auf den Chip. Bei einem dritten Chip wächst er mit.

### Option B: Klassenhierarchie (`KnxUartDriver` als Basis, `KnxTpUart2Driver` / `KnxNcn5130Driver`)
- \+ Saubere Trennung, chipspezifische Zusatzfunktionen (Systemstatus, Watchdog) haben einen eigenen Ort.
- − Die Auswahl **im Konstruktor** geht ohne Heap nur, wenn `KonnextraKNX` **beide** Treiber als
  Member hält (≈ 40 B RAM zusätzlich, auf dem Uno mit 2 KB spürbar), oder über ein Template
  (`KonnextraKNX<Ncn5130> knx`). Das Template würde die Nutzer-API und alle Beispiele brechen, und mit
  C++11 auf AVR gibt es kein `KonnextraKNX knx` ohne `<>`.
- − Mehr Doxygen-Fläche, mehr virtuelle Aufrufe auf dem Sendepfad.

### Option C: Auswahl zur Compile-Zeit (`-DKNX_TRANSCEIVER=TPUART2`)
- \+ Null Laufzeitkosten.
- − Erfüllt die Anforderung „im Konstruktor wählbar“ nicht. In der Arduino IDE sind Build-Flags
  für Einsteiger praktisch nicht setzbar.

**Empfehlung: Option A.** Der Unterschied liegt fast nur in Daten und einer Abfolge, nicht im
Verhalten. Weil das Protokoll ausgelagert ist, bleibt ein späterer Umbau auf B billig, falls die
chipspezifischen Funktionen wachsen.

---

## 3. Klassenentwurf (Option A)

### 3.1 Überblick

```
Sketch
  └─ KonnextraKNX : KnxCoordinator            (KonnextraKNX.h)   ← neue Konstruktor-Variante
        └─ owns KnxDriver : IKnxDriver        (KnxDriver.h/.cpp) ← Member transceiver, neue Inbetriebnahme
              ├─ owns KnxBringUp              (NEU, ohne Arduino, Host-getestet): Inbetriebnahme-Automat
              ├─ uses KnxTransceiverProtocol  (NEU, ohne Arduino, Host-getestet)
              │     └─ KnxTransceiver enum    (KnxEnums.h, NEU, öffentlich)
              └─ KnxReassembler               (unverändert)
```
`IKnxDriver`, `KnxCoordinator`, `KnxFrame` und alle Gerätetypen bleiben **unverändert**. `KnxReassembler` bekommt nur den Getter `inFrame()`.
Wer seinen eigenen `IKnxDriver` injiziert, ist nicht betroffen.

### 3.2 `KnxTransceiver` (KnxEnums.h, öffentlich)

```cpp
/**
 * @brief Which bus coupler chip the UART is connected to.
 */
enum class KnxTransceiver : uint8_t {
	Ncn5130,   // onsemi NCN5130 / NCN5120 (KONNEXTRA BCU light, NanoBCU, ...) — default
	TpUart2    // Siemens TP-UART2 and compatibles (incl. the STKNX/ATtiny bridge)
};
```
- Der Name beschreibt die Rolle, nicht das Produkt. Die NCN5120 ist laut Datenblatt-Familie
  befehlsgleich. Vor einer Erwähnung in der Doku muss das gegen ihr Datenblatt geprüft werden.
- Doxygen: Er darf **nicht** in `EXCLUDE_SYMBOLS` landen. `KnxEnums.h` steht bereits in `INPUT`.
- `keywords.txt`: `KnxTransceiver` als KEYWORD1, `Ncn5130` und `TpUart2` als LITERAL1.

### 3.3 `KnxTransceiverProtocol` (NEU: `src/KnxTransceiverProtocol.h/.cpp`, ohne Arduino)

Ein zustandsloser Namespace wie `KnxFrame`. Er bündelt **alle** Protokollkonstanten, die heute
privat in `KnxDriver.h` stehen, und ergänzt die NCN5130-spezifischen.

```cpp
namespace KnxTransceiverProtocol {
	//---- Host → chip (common) ----
	constexpr uint8_t U_RESET_REQ           = 0x01;
	constexpr uint8_t U_STATE_REQ           = 0x02;
	constexpr uint8_t U_DATA_START_CONTINUE = 0x80;
	constexpr uint8_t U_DATA_END            = 0x40;
	//---- Host → chip (chip-specific) ----
	constexpr uint8_t TPUART2_SET_ADDRESS   = 0x28;   // + hi, lo
	constexpr uint8_t NCN5130_SET_ADDRESS   = 0xF1;   // + hi, lo, dummy
	constexpr uint8_t NCN5130_SYSTEMSTAT    = 0x0D;   // → 0x4B + status byte

	//---- Chip → host ----
	constexpr uint8_t U_RESET_IND           = 0x03;
	constexpr uint8_t U_STATE_IND_OK        = 0x07;
	constexpr uint8_t NCN5130_SYSTEMSTAT_IND = 0x4B;

	// Longest set-address command over all chips (NCN5130: 4 bytes).
	constexpr uint8_t SET_ADDRESS_MAX_LEN   = 4;

	// Writes the set-address command for this chip into out, returns its length (3 or 4).
	uint8_t buildSetAddress(KnxTransceiver chip, PhysicalAddress address, uint8_t* out);

	// Bring-up timing per chip.
	uint16_t resetTimeoutMs(KnxTransceiver chip);        // TP-UART2: 20, NCN5130: 200
	// True if the chip confirms the address with U_Configure.ind (NCN5130 only).
	bool confirmsAddress(KnxTransceiver chip);
	uint16_t addressConfirmTimeoutMs(KnxTransceiver chip); // NCN5130: 100 (waits for bus idle)

	// Byte classification — shared, chip-independent.
	bool isResetIndication(uint8_t b);
	bool isStateIndication(uint8_t b);            // (b & 0x07) == 0x07
	bool isConfirmation(uint8_t b);               // (b & 0x7F) == 0x0B
	bool isPositiveConfirmation(uint8_t b);       // b & 0x80
	bool isConfigureIndication(uint8_t b);        // (b & 0x83) == 0x01
	bool autoAcknowledgeActive(uint8_t configureInd); // bit 5 (aa)

	// NCN5130 U_SystemStat.ind second byte, decoded.
	struct SystemStatus {
		bool v20v, vdd2, vbus, vfilt, xtal, thermalWarning;
		uint8_t mode;          // 0 Power-Up, 1 Sync, 2 Stop, 3 Normal
	};
	SystemStatus decodeSystemStatus(uint8_t statusByte);
}
```
- Die Zeitlimits sind Startwerte, die die Hardwaretests (Abschnitt 5) bestätigen oder anpassen.
  200 ms für den NCN5130 ergeben sich so: 8 ms Reset, mindestens 40 Tbit Ruhe (≈ 4,2 ms), und
  großzügig Reserve für einen belebten Bus.
- `PhysicalAddress` stammt aus `KnxAddress.h`. Der Struct liegt außerhalb von `#ifdef ARDUINO`
  (geprüft). Nur `physicalAddressFromString()` braucht `String`, also kommt das Modul ohne Arduino aus.

### 3.3a `KnxBringUp` (NEU: `src/KnxBringUp.h/.cpp`, ohne Arduino): Inbetriebnahme als Zustandsautomat

**Warum:** Der NCN5130 (und genauso der TP-UART2) reicht Busframes jederzeit an den Host weiter, auch
während der Inbetriebnahme. `U_Configure.ind` kommt laut Datenblatt erst, wenn der Bus frei ist. Ein
laufendes Frame kommt also **vorher** über die UART. Wer dann nur Bytes nach Muster sucht, wie das
heutige `stateRequest()`, findet Treffer in den Nutzdaten:
- `(b & 0x83) == 0x01` passt auf `0x01`, `0x05`, `0x11` und `0x21`. Das sind typische Adress- und Datenbytes.
- `(b & 0x07) == 0x07` und `0x4B` haben dasselbe Problem.

Das ist ein Fehler in der **Abfolge**, nicht in den Bytes, und er gehört deshalb in Host-getesteten Code.

**Regel:** Ein Byte ist nur dann eine Antwort des Chips, wenn **kein Frame gerade empfangen wird** und das
Byte **kein** L_Data-Steuerbyte ist. Das lässt sich am Reassembler ablesen. Keiner der Antwortcodes
(`0x03`, `0x07`–`0xFF`-Status, `0x21`, `0x4B`, `0x8B`/`0x0B`) passt auf das Steuerbyte-Muster
`(b & 0xD3) == 0x90`. Ausnahme: Das Statusbyte nach `0x4B` ist beliebig und wird positionsgebunden gelesen.

```cpp
class KnxBringUp {
	public:
		enum class Step : uint8_t { Reset, SystemStatus, SetAddress, AwaitConfigure, State, Done, Failed };

		// Starts a bring-up for this chip and address at time nowMs.
		void start(KnxTransceiver chip, PhysicalAddress address, uint32_t nowMs);
		// Command the driver must write next, if any (consumed on read).
		bool nextCommand(const uint8_t*& cmd, uint8_t& len);
		// Feeds one received byte. Bytes inside a bus frame are skipped, never matched.
		void feed(uint8_t byte, uint32_t nowMs);
		// Advances timeouts; call regularly while not finished.
		void tick(uint32_t nowMs);

		bool finished(void) const;          // Done or Failed
		bool succeeded(void) const;         // Done
		Step failedAt(void) const;          // which step timed out or answered wrongly
		KnxTransceiverProtocol::SystemStatus systemStatus(void) const;  // NCN5130 only
		uint8_t stateByte(void) const;
	private:
		KnxReassembler frameGuard;          // only used to know "inside a frame or not"
		// ... step, deadline, chip, address, command buffer (SET_ADDRESS_MAX_LEN)
};
```
- `KnxReassembler` bekommt dafür einen Getter `bool inFrame(void) const`. Es ist die einzige Änderung dort.
- Frames, die während der Inbetriebnahme fertig werden, werden verworfen. Es sind noch keine Empfänger aktiv.
- Der Treiber pumpt nur noch Ein- und Ausgabe (siehe 3.4). Die Logik ist Host-testbar ohne `ArduinoFake`,
  nach demselben Muster wie `KnxReassembler`.
- Die Abfolge je Chip steht in der Tabelle unter 3.4.

### 3.4 `KnxDriver` (geändert)

Öffentlich, jeweils mit **Default-Argument**. Bestehende Sketches kompilieren damit unverändert:
```cpp
KnxDriver(String physicalAddress, KnxTransceiver chip = KnxTransceiver::Ncn5130);           // nur mit KNX_DEFAULT_PORT
KnxDriver(String physicalAddress, HardwareSerial& port, KnxTransceiver chip = KnxTransceiver::Ncn5130);
KnxDriver(String physicalAddress, Stream& stream,     KnxTransceiver chip = KnxTransceiver::Ncn5130);

/** @brief The bus coupler chip this driver was configured for. */
KnxTransceiver transceiver(void) const;
```
Die Variante `= delete` für Boards ohne zweite UART (Uno) bekommt dieselbe Signatur.

Privat:
```cpp
KnxTransceiver chip;   // fixed at construction
KnxBringUp     bringUpSm;

// Shared by begin() and reset(): runs KnxBringUp to completion, pumping its commands to the
// port and RX bytes into it, then logs each step's outcome. Returns bringUpSm.succeeded().
bool bringUp(const char* phase);
```
Entfällt:
- die privaten Protokollkonstanten und `isConfirmation()` / `isPositiveConfirmation()` (wandern nach `KnxTransceiverProtocol`)
- `resetRequest()`, `stateRequest()`, `applyPhysicalAddress()` und `clearBuffer()` (ersetzt durch `KnxBringUp`)

**Neue Abfolge in `begin()` und `reset()`:**

| Schritt | TP-UART2 | NCN5130 |
|---|---|---|
| 1 Port | 19200 8E1 (nur auf dem HardwareSerial-Pfad) | gleich |
| 2 `U_Reset.req` → `U_Reset.ind` | Zeitlimit 20 ms (wie heute) | Zeitlimit 200 ms |
| 3 Systemstatus | – | `0x0D` → `0x4B` + Byte. Im Debug-Log stehen dann die Spannungsschienen und der Modus. Ist der Modus nicht Normal, schlägt `begin()` mit verständlichem Log fehl |
| 4 Adresse | `0x28 hi lo` | `0xF1 hi lo 0x00` |
| 5 Adresse bestätigt | – | Warten auf `U_Configure.ind` mit gesetztem `aa`-Bit, 100 ms |
| 6 `U_State.req` → `0x07` | gleich | gleich. **Erst nach Schritt 5**, damit das `U_Configure.ind` nicht als Statusantwort gelesen wird (die heutige Race) |
| Ergebnis | reset && state | reset && sysstat && configure && state |

Auch im Debug-Modus kommt eine Zeile zu jedem Schritt (`DRV ncn5130 sysstat: vbus ok vfilt ok … mode normal`,
`DRV ncn5130 address 1.1.5 confirmed (auto-ack on)`). Läuft der falsche Chip, steht im Log
ein Hinweis: „no Configure.ind — is this really an NCN5130? For TP-UART2 pass KnxTransceiver::TpUart2“.

Unverändert bleiben `sendTelegram()`, `awaitConfirmation()` (nutzt die Einordnung aus dem Namespace)
und `poll()`.

### 3.5 `KonnextraKNX` (geändert)

```cpp
explicit KonnextraKNX(const String& physicalAddress,
                      KnxTransceiver chip = KnxTransceiver::Ncn5130);          // KNX_DEFAULT_PORT
KonnextraKNX(const String& physicalAddress, HardwareSerial& port,
             KnxTransceiver chip = KnxTransceiver::Ncn5130);
KonnextraKNX(const String& physicalAddress, Stream& stream,
             KnxTransceiver chip = KnxTransceiver::Ncn5130);
```
Beispiele für die Nutzung:
```cpp
KonnextraKNX knx("1.1.5");                                    // NCN5130 auf Serial1
KonnextraKNX knx("1.1.5", KnxTransceiver::TpUart2);           // TP-UART2 auf Serial1
KonnextraKNX knx("1.1.5", Serial2, KnxTransceiver::TpUart2);  // TP-UART2 auf Serial2
```
- Mehrdeutigkeit prüfen: `("1.1.5", Serial1)` gegen `("1.1.5", KnxTransceiver)` sind verschiedene Typen,
  also unproblematisch. Die bestehende Unterscheidung zwischen `HardwareSerial&` und `Stream&` bleibt.
- Die Kopie in `KnxDriver` und `KonnextraKNX` bleibt bestehen. Die Meldung beim Uno, die Nutzer sehen, steht in `KonnextraKNX`.
- Doxygen: Die `PREDEFINED`-Einstellungen bleiben gleich, die neuen Parameter brauchen `@param chip`.

### 3.6 Bewusst **nicht** im ersten Schritt (YAGNI)
- Öffentliche API für den Systemstatus, den Watchdog, SAVEB und RESETB. Notiert für später, z.B.
  `knx.busVoltageOk()`.
- Auto-Erkennung des Chips. Möglich wäre: erst `0x0D` schicken, antwortet `0x4B`, ist es ein NCN5130.
  Was ein TP-UART2 mit `0x0D` macht, ist undokumentiert, deshalb vorerst nicht. Als Idee für 0.3 notiert.
- Busmonitor-Modus in der Library. Für das Testwerkzeug genügt ein eigener Sketch (5.4).

---

## 4. Schritte (Umsetzung, später auf `feature/ncn5130`)

1. **Branch anlegen** `feature/ncn5130` von `main`.
2. **TDD Protokoll:** `test/test_transceiver/test_transceiver.cpp` zuerst schreiben (5.1), dann
   `KnxTransceiverProtocol.h/.cpp`. Die native Umgebung nimmt das Modul automatisch mit, weil es
   ohne Arduino auskommt und `build_src_filter` nur `KnxDriver.cpp` ausschließt.
3. **`KnxTransceiver`** in `KnxEnums.h`.
3a. **TDD Inbetriebnahme:** zuerst `test/test_bringup` schreiben (5.1), dann `KnxBringUp` und `KnxReassembler::inFrame()`.
4. **`KnxDriver` umbauen:** Konstanten raus, `chip`-Member, `bringUp()` pumpt `KnxBringUp`.
   Sende- und Empfangspfad nicht anfassen.
5. **`KonnextraKNX`-Konstruktoren** um den Parameter `chip` ergänzen.
6. **Beispiele:**
   - `ExplicitPort.ino` bekommt eine kommentierte TP-UART2-Zeile.
   - `BenchTest.ino` bekommt eine `#define`-Umschaltung zwischen den Chips.
   - Die Matrix in der CI kompiliert damit beide Konstruktor-Varianten auf allen Cores.
7. **Testwerkzeuge** unter `extras/bench/` (5.3–5.5).
8. **Hardwaretests** nach 5.6. Die Ergebnisse und Logs kommen in `.agents/bench/2026-xx-xx-ncn5130.md`.
9. **Doku** (Abschnitt 6) und `Changes.md`.
10. **Release 0.2.0** nach dem bekannten Ablauf, mit dem Hinweis auf den geänderten Standard.

---

## 5. Verification (Teststrategie)

Vier Ebenen. Jede fängt eine andere Fehlerklasse ab:

| Ebene | Was | Fängt | Läuft |
|---|---|---|---|
| 5.1 Host-Unit | `KnxTransceiverProtocol` | falsche Bytes, falsche Einordnung | `pio test -e native`, CI |
| 5.2 Compile-Matrix | alle Beispiele × alle Cores | Überladungs- und Portabilitätsfehler | CI `portability` |
| 5.3–5.5 Werkzeuge | Sketch für zwei Boards, Busmonitor-Sketch, xknx-Mithörer, Python-Testablauf | – | lokal |
| 5.6 Hardware | Testmatrix mit beiden Chips, zwei Boards, xknx | echtes Verhalten am Bus, Empfang, Zeitverhalten | Testkoffer |

### 5.1 Host-Unit-Tests (`test/test_transceiver`)
- `buildSetAddress(TpUart2, 1.1.5)` → `28 11 05`, Länge 3.
- `buildSetAddress(Ncn5130, 1.1.5)` → `F1 11 05 00`, Länge 4.
- **Regression „Busmon-Falle“:** Für NCN5130 ist Byte 0 **nie** `0x28` (über eine Tabelle von
  Adressen: 1.1.1, 1.1.5, 15.15.255, 0.0.0).
- Randadressen: `15.15.255` → hi `0xFF`, lo `0xFF` bei beiden Chips korrekt.
- Einordnung:
  - `isConfigureIndication`: `0x21` und `0x01` → true; `0x03`, `0x07`, `0x0B`, `0x8B`, `0xBC` → false.
  - `autoAcknowledgeActive(0x21)` → true, `(0x01)` → false.
  - `isConfirmation`: `0x8B` und `0x0B` → true; `0x21` und `0x13` → false.
  - `isStateIndication(0x07)` → true, `(0x0F)` mit Fehlerbits → true (Fehler werden getrennt ausgewertet).
- `decodeSystemStatus`: Bitmuster gegen Datenblatt-Figur 38. Ein Testfall je Flag, Modus 0 bis 3.
- Zeitlimits: `resetTimeoutMs(Ncn5130) ≥ 100`, `confirmsAddress(TpUart2) == false`.
- **Bestehende Suites** (`test_reassembler`) bekommen einen Fall dazu: Ein `0x21`
  (Configure.ind), der vor einem Frame ankommt, wird ignoriert und stört das nächste Frame nicht.

**`test/test_bringup`** (Abfolge, mit Drehbuch-Bytes und künstlicher Zeit, ohne Arduino):
- NCN5130 ruhiger Bus: `01` → `03`, `0D` → `4B C7` (Normal), `F1 11 05 00` → `21`, `02` → `07` → Done.
  Geprüft werden die gesendeten Befehle in genau dieser Reihenfolge.
- TP-UART2: `01` → `03`, `28 11 05`, `02` → `07` → Done, ohne Systemstatus und ohne Warten auf Configure.ind.
- **Frame mitten in der Inbetriebnahme:** Zwischen `F1…` und `21` kommt ein vollständiges L_Data-Frame, dessen
  Nutzdaten `21`, `07` und `4B` enthalten. Erwartet: Kein falscher Treffer, Done erst nach dem echten `21`.
- Dasselbe zwischen `02` und `07`. Das ist die heutige Race in `stateRequest()`.
- Zeitüberschreitungen: kein `03` → Failed bei Reset. Kein `21` → Failed bei AwaitConfigure. Systemstatus-Modus
  Sync statt Normal → Failed bei SystemStatus. Jeweils mit richtigem `failedAt()`.
- `0x00` (UART-Leerlauf) wird überall ignoriert.
- Configure.ind mit `aa = 0` (`0x01`) → Failed. Die Adresse wurde gesetzt, aber Auto-ACK läuft nicht.

Bewusst nicht vorgesehen ist ein Mock-Stream für den ganzen `KnxDriver` (`ArduinoFake`). Nach dem Umbau ist der
Treiber nur noch Ein- und Ausgabe, und die Logik steckt in `KnxBringUp`.

### 5.2 Compile-Matrix (CI)
- Die bestehende `portability`-Matrix deckt das ab, sobald die Beispiele beide Konstruktor-Varianten nutzen (Schritt 6).
- `uno-single-uart` muss weiter **fehlschlagen**, auch mit dem `chip`-Argument.
- Doxygen erzeugt keine Warnungen. Kontrolle: Der Konstruktor mit nur der Adresse erscheint weiter, jetzt mit `@param chip`.

### 5.3 Sketch für zwei Boards (`extras/bench/BenchPair/BenchPair.ino`)
Ein Sketch für beide Boards, die Rolle per `#define`:
- `ROLE_A` (Sender): alle 2 s abwechselnd DPT1 an `0/1/1` (an, aus) und DPT5 an `0/2/1` (Zähler 0–255).
  Optional mit Burst-Modus (20 Telegramme direkt hintereinander, auf Befehl über Serial).
- `ROLE_B` (Empfänger): `KnxObject`s auf `0/1/1`, `0/2/1` und auf eine Adresse, die xknx beschreibt (`0/3/1`),
  mit Debug-Modus für fremden Verkehr.
- `#define CHIP KnxTransceiver::Ncn5130` oder `TpUart2`, damit auch Paarungen aus beiden Chips gehen.
- **Maschinenlesbare Ausgabe**, eine Zeile pro Ereignis:
  ```
  BOOT role=A chip=ncn5130 pa=1.1.5 begin=1
  TX t=12345 ga=0/1/1 dpt=1 val=1 con=pos
  RX t=12400 src=1.1.6 ga=0/1/1 dpt=1 val=1
  FOREIGN t=… src=… ga=…           (aus dem Debug-Log, Verkehr ohne Empfänger)
  ```
- Physikalische Adressen: A = 1.1.5, B = 1.1.6. Kollisionen mit dem Testkoffer vorher prüfen.
- Liegt unter `extras/`, **nicht** unter `examples/`. `extras/` ist laut Arduino-Library-Spezifikation
  für mitgelieferte Zusatzinhalte reserviert. Vor dem ersten Push prüfen, ob `arduino-lint` (Update-Modus)
  sich an einer `.ino` außerhalb von `examples/` stört. Damit erscheint er nicht in der Beispielliste der
  IDE und nicht auf der Website. Die CI soll ihn trotzdem kompilieren, dafür bekommt `ci.yml` einen eigenen Eintrag.

### 5.4 Busmonitor-Sketch (`extras/bench/BusMonitor/BusMonitor.ino`, optional, drittes Board)
- Ohne Library. Er öffnet 19200 8E1, schickt `0x01` (Reset), wartet auf `0x03` und schickt `0x05` (`U_Busmon.req`).
- Danach gibt er jedes Byte mit µs-Zeitstempel als Hex aus. Eine Lücke von mehr als 2 ms im Bytestrom beendet die Zeile.
- Zeigt, was xknx nicht kann: **ACK-Frames (`0xCC` ACK, `0x0C` NACK, `0xC0` BUSY), Wiederholungen,
  Rohzeiten**. Damit lässt sich beweisen, wer wann bestätigt.
- Begründung aus dem Datenblatt: Der NCN5130 reicht im Busmonitor ACK-Frames transparent durch.
- Masse-Regeln beachten: Das Board ist nicht galvanisch getrennt, am Mac also nur ohne geerdetes Netzteil betreiben.

### 5.5 xknx-Mithörer und Testablauf (`extras/bench/`)

**Netzwerk (Option 1, entschieden):** Ethernet-Kabel vom Router im Testkoffer an den Mac
(USB-C-Adapter). In Systemeinstellungen → Netzwerk → „…“ → *Reihenfolge der Dienste festlegen*
steht **WLAN über dem Ethernet-Adapter**. Dann läuft das Internet weiter über WLAN, und nur das
Subnetz des Koffers geht über das Kabel. Prüfen mit `route -n get default` (muss `en0`/WLAN zeigen) und
`route -n get <IP der Schnittstelle>` (muss den USB-Ethernet-Adapter zeigen).

**Werkzeuge:**
- `extras/bench/requirements.txt`: `xknx`, `pyserial`.
- `extras/bench/knx_listen.py`: Tunnel zur IP-Schnittstelle, jedes Telegramm als Zeile
  (gleiches Format wie oben, Präfix `IP`) auf stdout und in `logs/<zeitstempel>-ip.log`.
- `extras/bench/run_pair.py`: liest **zwei serielle Ports passiv**, dazu den xknx-Tunnel
  gleichzeitig (asyncio). Er führt ein Szenario aus und ordnet die Ereignisse einander zu (Gruppenadresse + Wert +
  Zeitfenster ±500 ms). Am Ende steht eine PASS/FAIL-Tabelle plus CSV.
  - Lehre vom C6: Ports mit `dtr=False, rts=False` **vor** `open()` öffnen. Sonst setzt jedes
    Öffnen den Chip zurück und kann ihn im Download-Modus hängen lassen.
- Ich (Claude) kann `knx_listen.py` im Hintergrund laufen lassen und die Logs während der Tests
  mitlesen, weil die Internetverbindung über WLAN bestehen bleibt.

#### 5.5.1 Recherche xknx (Stand 21.11.2026, gegen Quellcode `XKNX/xknx` main und PyPI)

**Aufwand: klein.** Einrichtung und erster Mitschnitt dauern zusammen etwa 30 Minuten.
- Version **xknx 4.1.1** (05.11.2026), braucht **Python ≥ 3.11**. Mindestens 4.1.1 verwenden, weil darin
  Fehler bei Reconnect und Sequenzzähler behoben sind.
- **Achtung, dieser Mac:** `python3` ist das System-Python 3.9.6 und zu alt. pip würde dann still eine
  ältere xknx-Version mit anderer API installieren. Vorhanden ist Homebrew `python3.12` (geprüft). Deshalb:
  `/opt/homebrew/bin/python3.12 -m venv extras/bench/.venv && extras/bench/.venv/bin/pip install -r extras/bench/requirements.txt`.
  In `requirements.txt` steht `xknx>=4.1.1` und `pyserial`, und `.venv` kommt in die `.gitignore`.
- Seit 4.0 gibt es eine **eigene Kommandozeile**. Für einen schnellen Blick reicht schon:
  `xknx group monitor --gateway <IP>` (bzw. `XKNX_GATEWAY` und `XKNX_LOCAL_IP` als Umgebungsvariablen),
  dazu `xknx group write 0/3/1 on` und `xknx group read 1/1/1`. Das Skript wird erst für die
  Zuordnung und die Logs gebraucht.

**Kern von `knx_listen.py`** (die API ist geprüft):
```python
cfg = ConnectionConfig(connection_type=ConnectionType.TUNNELING,   # TUNNELING_TCP, falls die Schnittstelle Core v2 kann
                       gateway_ip=GW, local_ip=ETH_IP_OR_IFNAME)    # local_ip auf den Ethernet-Adapter festlegen
xknx = XKNX(connection_config=cfg, daemon_mode=True)
xknx.telegram_queue.register_telegram_received_cb(on_telegram, match_for_outgoing=True)
await xknx.start(); ...; await xknx.stop()   # stop() immer, sonst bleibt der Tunnelplatz ~120 s belegt
```
`local_ip` akzeptiert auch einen Interface-Namen (`"en7"`). Damit ist ausgeschlossen, dass der Tunnel über WLAN aufgebaut wird.

**Was xknx sieht und was nicht:**

| Sichtbar | Nicht sichtbar |
|---|---|
| Gruppentelegramme aller TP-Geräte, also auch von Board A und B (als `L_Data.ind`) | **TP-ACK, NACK und BUSY.** Ein Tunnel auf Sicherungsschicht liefert grundsätzlich keine Schicht-2-Quittungen |
| Quelle, Ziel, APCI, Payload | Zeitstempel (selbst beim Empfang setzen) |
| Über einen Hook auf `xknx.cemi_handler.handle_raw_cemi`: rohe cEMI-Bytes mit `CEMIFlags` (Hop-Count, Priorität, Repeat-Flag) | Telegramme an physikalische Adressen landen nicht im Gruppen-Callback. Sie sind nur über den Roh-Hook sichtbar |
| eigene Sendungen (mit `match_for_outgoing=True` als OUTGOING) | Extended Frames (werden verworfen) |
| | Wiederholungen: Viele Schnittstellen filtern sie vor dem IP-Netz heraus. Das ist unbestätigt, wir messen es |

**Busmonitor über IP:** xknx unterstützt **keinen** Busmonitor-Tunnel. Es kennt nur Data-Link- und Raw-Layer,
kein `L_Busmon.ind`. Viele IP-Schnittstellen können es ohnehin nicht (bekannt bei Weinzierl 730), und bei
IP-*Routern* verbietet die Spezifikation es. **Deshalb ist der Busmonitor-Sketch (5.4) die einzige billige Quelle
für ACKs und Wiederholungen.** Alternativen sind der ETS-Busmonitor (wenn die Schnittstelle des Koffers es kann)
oder knxd/`knxtool busmonitor1`, das auf dem Mac aber nur selbst kompiliert oder in einer VM läuft.

**xknx als Testpartner:** `group_value_write(xknx, "0/3/1", True)` und `group_value_read(...)` aus `xknx.tools`.
Das Ergebnis wird gegen die `L_Data.con` der Schnittstelle geprüft. Damit lassen sich H6 und H9 automatisch steuern.

**Stolpersteine:**
- **Tunnelplätze:** Schnittstellen bieten 1–8. ETS, Home Assistant und unser Skript belegen je einen. Wird eine
  Verbindung nicht sauber beendet, gibt es für ~120 s `E_NO_MORE_CONNECTIONS`.
- **Ethernet am Mac:** Eine **statische IP** im Subnetz des Koffers, **ohne Router und ohne DNS**. So bleibt die
  Standardroute sicher auf WLAN. Das ist robuster als nur die Reihenfolge der Dienste.
- **macOS-Firewall:** Ist „Alle eingehenden blockieren“ oder der Tarnmodus aktiv, muss Python erlaubt werden. Bei
  UDP-Tunneling kommen die Pakete auf dem lokalen Port an.
- `xknx scan` (Multicast) nutzt unter macOS evtl. die falsche Schnittstelle. Der Fix ist noch nicht released.
  Für Tunneling auf eine feste IP ist das egal, also die Gateway-IP direkt angeben.
- Hängt der Koffer hinter NAT (nicht bei Option 1): `route_back=True` oder TCP.

Quellen: pypi.org/pypi/xknx; github.com/XKNX/xknx (`xknx/io/connection.py`, `xknx/cemi/cemi_handler.py`,
`xknx/cemi/flags.py`, `xknx/knxip/knxip_enum.py`, `examples/example_telegram_monitor.py`, `docs/cli.md`,
`docs/changelog.md`); support.knx.org Artikel 360021696819 (Busmonitor-Unterstützung).

### 5.6 Hardware-Testmatrix

Aufbau: Testkoffer, IP-Schnittstelle, Board A (C6 + BCU light), Board B (zweiter µC + BCU
light), optional Board C als Busmonitor. Zum Gegentest ein TP-UART2 (Siemens oder STKNX-Board).

| # | Test | Aufbau | Erwartung (PASS) | Beweist |
|---|---|---|---|---|
| H1 | Inbetriebnahme NCN5130 | A, NCN-Profil, Debug an | Reset OK, Systemstatus Normal + alle Schienen OK, Configure.ind `aa=1`, State `0x07`, `begin()=1` | 3.4-Abfolge, Zeitlimits |
| H2 | Falsches Profil | A mit TpUart2-Profil auf NCN-Board | `begin()=0`, verständliche Logzeile. **Danach** mit NCN-Profil wieder OK (kein Stromtrennen nötig, weil der Reset den Busmonitor beendet) | Fehlerbild ist harmlos und erklärbar |
| H3 | Adressen | A mit 1.1.1, 1.1.5, 15.15.255 | jeweils Configure.ind, `begin()=1` | Busmonitor- und Watchdog-Falle beseitigt |
| H4 | Senden | A sendet, xknx hört | jedes TX auch im IP-Log, `con=pos`, Quelle 1.1.5 | Sendepfad am NCN5130 |
| H5 | **Empfang** | A sendet, B empfängt | jedes TX von A erscheint als RX bei B mit richtigem Wert und richtiger Quelle | **Empfang auf echter Hardware, die offene Beta-Lücke** |
| H6 | Empfang von fremdem Gerät | xknx schreibt `0/3/1` | B: RX mit Wert. Busmonitor: genau 1 Frame, 1 ACK, keine Wiederholung | Empfang + Auto-ACK |
| H7 | Bestätigung ohne Empfänger | A sendet an eine unbenutzte Gruppenadresse (`0/7/7`) | Busmonitor zeigt ACK von B (Auto-ACK bestätigt **alle** Gruppentelegramme). Daher ist `con=pos` hier **kein** Beweis für einen Empfänger | Dokumentiert eine Eigenschaft, die man kennen muss |
| H8 | Gemischt | A = NCN5130, B = TP-UART2 (jeweils mit passendem Profil) | H4 bis H6 in beide Richtungen PASS | **Beide Chips gleichzeitig** |
| H9 | Last | A-Burst 20× + xknx 20× gleichzeitig | Anzahl bei B = 40, alle Werte, keine Prüfsummenfehler, alle `con=pos`. Wiederholungen nur im Busmonitor sichtbar | Arbitrierung, Reassembler unter Last |
| H10 | Normaler Busverkehr | 30 min, nur der Koffer (Geräte des Koffers) | Die Anzahl der Frames bei B (FOREIGN + RX) entspricht dem xknx-Log (± Telegramme, die die Schnittstelle selbst filtert). 0 Fehler im Reassembler | Empfang bei realem Verkehr |
| H11 | Bus aus/an | Busspannung kurz trennen, µC bleibt versorgt | Senden schlägt währenddessen fehl. Nach `knx.reset()` (bzw. automatisch, falls geplant) wieder OK. Systemstatus zeigt VBUS | Erholung, Diagnose |
| H12 | Regression TP-UART2 | altes Board / Siemens, TpUart2-Profil, BenchTest | Verhalten identisch zu 0.1.9 | Nichts kaputt |
| H14 | Inbetriebnahme unter Last | xknx schickt Dauerfeuer (z.B. 10 Telegramme/s), währenddessen 20× `knx.reset()` an A | 20× OK, kein falsches Configure.ind oder State, Zeiten notieren | `KnxBringUp`-Regel auf echter Hardware |
| H13 | Gerät prüfen | ETS „Gerät prüfen“ / Device-Descriptor-Read auf 1.1.5 | Antwort kommt (Adressierung über die physikalische Adresse) | Auto-ACK für die eigene Adresse |

H5, H8 und H10 sind die drei Tests, die die Beta-Einschränkung „Empfang nicht bestätigt“ aufheben.

Messwerte, die notiert werden (für Zeitlimits und Doku):
- Dauer von `U_Reset.req` bis `U_Reset.ind`: Median und Maximum über 20 Resets, ruhiger und belebter Bus.
- Dauer von `U_SetAddress` bis `U_Configure.ind`.
- Dauer vom Senden bis `L_Data.con`.

---

## 6. Doku und Folgearbeiten

| Ort | Änderung |
|---|---|
| `README.md` | Hardware-Abschnitt (STKNX/ATtiny/ISO7721) ersetzen durch BCU light (NCN5130, 3,3 V, nicht galvanisch getrennt, Masse-Warnung). Hinweis auf TP-UART2-Profil |
| `library.properties` `paragraph` | „ATTiny/STKNX co-processor link“ → „NCN5130 or TP-UART2 bus coupler“ |
| `CLAUDE.md` | Abschnitt „Physical layer“ neu. Pin-Map um Header-Belegung ergänzen (1 5V, 2 3V3, 3/4 GND, 5 RXD, 6 TXD, 7 RESETb, 8 SAVEb). TXD des Headers = Ausgang des Chips → RX des µC |
| `KnxDriver.h` Kopfkommentar | „STKNX-behind-ATTiny“ → beide Chips |
| `KnxInterfaces.h` Kommentar | „(ATTiny co-processor / TP-UART)“ anpassen |
| Website (`Website_`, en + de) | Getting Started (Konstruktor mit Chip), Supported Boards (3,3 V direkt, 5 V nur mit Pegelwandler, Masse-Hinweis), Release Notes 0.2.0 |
| `keywords.txt` | neue Bezeichner |
| `Changes.md` | Standard-Chip geändert (Sketch-relevant!), neuer Konstruktorparameter, neue Logzeilen |
| `.agents/handoff/library-release-0.1.9.md` | Hinweis auf die Hardware steht schon drin |
| Forum | Ergebnis von H5/H8 als Update im KNX-User-Forum-Thread, Frage #15 (NanoBCU) beantworten |

---

## 7. Risiken und offene Punkte

- **Reaktion des TP-UART2 auf `0xF1`** (falls jemand das Profil vergisst): Das Datenblatt nennt keinen
  Befehl `0xF1`. Erwartet ist ein Protokollfehler (`U_State.ind` mit PE, „illegal controlbyte“), das ist aber nicht verifiziert. Wird in H2-gespiegelt
  (NCN-Profil auf dem TP-UART2-Board) gemessen und in der Doku beschrieben.
- **Echo bei Wiederholungen:** Ob der NCN5130 bei einer internen Wiederholung das Frame erneut zurückschickt,
  steht nicht eindeutig im Datenblatt. Falls ja, landen die Bytes als „unexpected byte“ im Log
  (bekanntes Rauschen) oder im Reassembler. H9 mit Busmonitor klärt das.
- **Auto-ACK auf alle Gruppentelegramme:** Jedes Board mit BCU light bestätigt jedes
  Gruppentelegramm. Bei mehreren eigenen Geräten an einer Linie ist das KNX-konform, verdeckt aber
  bei Tests, ob wirklich jemand zuhört (siehe H7).
- **Stromversorgung beim Einschalten:** Der µC ist eigenversorgt, der NCN5130 kommt erst mit Busspannung. Startet der µC
  zuerst, schlägt `begin()` fehl (RESETB low, keine UART). Ein automatischer Neuversuch ist
  nicht geplant. Das Beispiel zeigt eine Schleife mit Wiederholung bzw. `knx.reset()`. Wird in H11 bewertet.
- **Galvanisch nicht getrennt:** Testaufbau ausschließlich mit MacBook ohne geerdetes Netzteil oder
  Hub. Für Kunden kommt ein deutlicher Hinweis in README und Website.
