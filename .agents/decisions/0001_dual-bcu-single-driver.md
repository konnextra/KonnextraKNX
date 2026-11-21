# 0001 — Beide BCUs mit einem Treiber, Chip per Enum, Standard NCN5130

Datum: 19.11.2026 (Entscheidung Florian), umgesetzt 21./22.11.2026.

## Kontext
Die neue KONNEXTRA BCU light nutzt den onsemi NCN5130 statt STKNX/ATtiny (TP-UART2-Protokoll).
Beide Chips sind im Sendepfad, Echo und `L_Data.con` gleich, unterscheiden sich aber beim
Setzen der Adresse und in der Inbetriebnahme (Details: `context/ncn5130.md`). Bestehende
Nutzer haben TP-UART2-Hardware.

## Entscheidung
- Die Library unterstützt **beide** Chips („zweischienig“).
- **Ein** `KnxDriver`; der Chip wird als `KnxTransceiver` (Enum) am Konstruktor gewählt,
  letzter Parameter mit Default **`KnxTransceiver::Ncn5130`**.
- Protokollwissen in `KnxTransceiverProtocol` (zustandslos), Inbetriebnahme in `KnxBringUp`
  (Zustandsautomat); beide Arduino-frei und host-getestet.
- Verworfen: Klassenhierarchie je Chip (RAM für beide Treiber im `KonnextraKNX`-Objekt oder
  Template-API) und Auswahl per Compile-Zeit-Makro (unpraktisch in der Arduino IDE).

## Konsequenzen
- **Breaking** für TP-UART2-Nutzer: sie müssen `KnxTransceiver::TpUart2` angeben, sonst
  schlägt `begin()` fehl → Release **0.2.0** mit deutlichem Hinweis.
- Alte Race behoben: Bytes in Busframes während der Inbetriebnahme gelten nicht als Antwort.
- Kein Auto-Detect, keine öffentliche Systemstatus-API (YAGNI, ggf. 0.3).
- Spec: `.agents/specs/2026-11-21-ncn5130-dual-bcu-design.md`.
