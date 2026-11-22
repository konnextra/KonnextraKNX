# 0003 — Reset-/Save-Pins optional, Selbstheilung nach Chip-Reset immer

Datum: 22.11.2026 (Entscheidung Florian: „Ja, so machen“).

## Kontext
NCN5130: RESETB und SAVEB sind **Ausgänge** (Open-Drain, Pull-up auf VDDD 3,3 V). TP-UART2: RESn
ist **Ein- und Ausgang** (Open-Drain, Pull-up auf VIF; extern low ziehen = Reset), SAVE ist
Ausgang (low bei Busspannungseinbruch > ~1,5 ms). Nach einem Reset (z.B. Spannungseinbruch)
verliert der Chip die physikalische Adresse → kein Auto-ACK, ohne dass die Library es merkte.

## Entscheidung
- **Immer (ohne Pins):** Ein `U_Reset.ind` außerhalb eines Frames in `poll()` löst einen neuen
  Bring-up aus.
- **Optional:** `knx.setStatusPins(resetPin, savePin)` (vor `begin()`, -1 = nicht verdrahtet),
  `knx.onBusPowerFail(cb)`, `knx.busPowerOk()`.
  - `begin()`/`reset()` warten bis Reset-Pin high (1 s Timeout). TP-UART2 zusätzlich: 2-ms-Puls
    low auf RESn vorher, danach 50 ms warten (Datenblatt).
  - Reset-Pin low → kein Senden, kein Lesen; steigende Flanke → Bring-up „recover“.
  - Fallende SAVE-Flanke → Callback (aus `loop()`).
- Pins nur `INPUT`, nie HIGH getrieben, kein MCU-Pull-up (5-V-Pull-up würde die Leitung über die
  Chip-Versorgung heben). 5-V-Boards brauchen Pegelwandler.
- **Doku muss den Unterschied klar sagen:** beim TP-UART2 setzt der Pin den Chip zurück, beim
  NCN5130 wird er nur gelesen (Florians ausdrücklicher Wunsch).

## Konsequenzen
- Arduino-only Code in `KnxDriver.cpp`, nicht host-getestet; braucht Hardwaretest (Pins an
  Header 7/8 der BCU light, Busspannung kurz trennen).
- Fehlalarm möglich, wenn Echo-Bytes nach einem Con-Timeout ein `0x03` außerhalb eines Frames
  liefern → nur ein unnötiger Bring-up, harmlos.
- Ob der NCN5130 nach Spannungsrückkehr spontan `U_Reset.ind` schickt, ist **ungeprüft**; mit
  Reset-Pin greift die Flankenerkennung unabhängig davon.
