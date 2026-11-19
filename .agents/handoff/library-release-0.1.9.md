# Handoff: KonnextraKNX im Arduino Library Manager (Release 0.1.9)

Zusammenfassung für die Session, in der der Blogpost entsteht. Stand: 12.11.2026.
Alle Uhrzeiten in MEZ/MESZ (Wien), UTC in Klammern, wo sie aus Logs stammt.

> **Hardware-Stand beachten:** Die Hardware-Angaben unten (STKNX + ATtiny, galvanisch getrennt)
> beschreiben die Zeit des Releases. Seit November 2026 ist die BCU ein **NCN5130** („KONNEXTRA BCU
> light“, 3,3 V, nicht galvanisch getrennt). Den aktuellen Stand vor dem Schreiben prüfen.

## Kernaussage

**Am 7. Oktober 2026 wurde KonnextraKNX in den Arduino Library Manager aufgenommen.** Seither
ist die Library in der Arduino IDE über *Tools → Manage Libraries…* (Suche „KonnextraKNX“), per
`arduino-cli lib install KonnextraKNX` und in der PlatformIO Registry als
`konnextra/KonnextraKNX` installierbar. Die erste und derzeit einzige öffentliche Version ist
**0.1.9, offiziell als erste Beta** bezeichnet.

## Zeitleiste

| Datum | Ereignis |
|---|---|
| 02.08.2026 | Machbarkeit geprüft: Die Namen `Konnextra`, `KonnextraKNX` und `Konnextra KNX` waren frei. Im ganzen Index (damals 9787 Libraries) erwähnten nur **zwei** Einträge KNX überhaupt (`KIMlib`, `KONNEKTING Device Library`). |
| 21.08.2026 | Repository auf das vom Library Manager verlangte Layout umgebaut: `library.properties` im Wurzelverzeichnis, eine flache `src/` statt sieben PlatformIO-Teilbibliotheken, Bench-Sketch nach `examples/BenchTest/`, `arduino-lint` in der CI. |
| 06.10.2026 | `v0.1.8` getaggt, erster Stand mit dem neuen Layout. User-Guides ins Website-Repo verschoben. |
| **07.10.2026, 14:47** | **`v0.1.9` getaggt.** Einziger Unterschied zu 0.1.8: Die KNX-Spezifikationen und das TP-UART2-Datenblatt (`reference/`, 19 MB urheberrechtlich geschützte PDFs) wurden aus dem Repo genommen, weil der Library Manager das ganze Repo mit jeder Installation ausliefert. |
| **07.10.2026, 14:52** | Registry-PR geöffnet: [arduino/library-registry#9305](https://github.com/arduino/library-registry/pull/9305), eine Zeile in `repositories.txt`. |
| **07.10.2026, 14:53 (12:53 UTC)** | **PR vom Registry-Bot geprüft und gemergt**, rund eine Minute nach dem Öffnen. Bot-Kommentar: „Your submission has now been accepted!“ |
| 08.10.2026, 07:13 (05:13 UTC) | Automatisch in die **PlatformIO Registry** übernommen (`konnextra/KonnextraKNX@0.1.9`), ohne eigenes Zutun. |
| 08.10.2026 | Im öffentlichen **Arduino-Library-Index** sichtbar (0.1.8 und 0.1.9). In der IDE erst nach einem Neuladen des lokalen Index sichtbar. |
| 08.10.2026, 16:55 | Entfernung von 0.1.8 beantragt (enthielt noch die PDFs): Tag gelöscht, Issue [arduino/library-registry#9315](https://github.com/arduino/library-registry/issues/9315). |
| 09.10.2026, 17:51 (15:51 UTC) | **0.1.8 von den Arduino-Maintainern manuell entfernt.** Seither ist 0.1.9 die einzige Version im Library Manager. |
| 13.10.2026 | Release Notes auf konnextra.at neu begonnen: Sie zählen ab dem ersten Library-Manager-Release, ältere Einträge wurden entfernt. |
| 12.11.2026 | Noch keine eigene Seite auf docs.arduino.cc (`/libraries/konnextraknx/` liefert 404). Diese Seiten baut Arduino unabhängig vom Index mit mehreren Wochen Verzögerung. |

## Was die Library kann (für den Blogpost)

- KNX-Bus aus einem Arduino-Sketch ansprechen, **ohne ETS**: Gruppenadressen stehen direkt im Sketch.
- Geräte werden über ihre Funktion beschrieben statt über Datenpunkttabellen: `KnxLight`,
  `KnxDimmLight`, `KnxRGB`, `KnxBlind`, `KnxTemperature`, `KnxHumidity`, `KnxPercent`,
  `KnxTime`, `KnxDate`, `KnxDateTime`, `KnxChar`, `KnxFloat`, dazu `KnxObject` für jeden anderen DPT.
- Ein vollständiger Sketch braucht nur `#include <KonnextraKNX.h>`, `KonnextraKNX knx("1.1.5");`,
  ein Geräteobjekt, `knx.begin()` und `knx.loop()`.
- Sendebefehle melden die echte Busbestätigung (`L_Data.con`), kein optimistisches `true`.
- Das Bit-Timing läuft auf einem ATtiny-Coprozessor (Konnextra Bridge, STKNX-Transceiver,
  galvanisch getrennt). Auf dem Haupt-Controller laufen keine zeitkritischen Interrupts, WiFi/MQTT/Matter
  können parallel laufen. Die UART-Schnittstelle spricht das TP-UART2-Protokoll, ein
  Standard-TP-UART2 funktioniert genauso.
- Die CI baut auf AVR, Renesas, Arduino Mbed, STM32duino, RP2040 und ESP32. Auf echter Hardware
  liefen bisher: XIAO ESP32-C6, ESP32-WROOM-32, Nucleo-L432KC, UNO R4 Minima, GIGA R1, Mega 2560,
  Uno R3, Pico 2.
- Lizenz BSD 3-Clause.

## Status: Beta — was der Blogpost ehrlich sagen sollte

- Das Senden ist auf echter Hardware verifiziert, mit positiver Busbestätigung. Die Examples
  DeviceObject, StatelessSend und CustomKnxObject liefen am 08.10. auf dem XIAO ESP32-C6.
- **Der Empfangspfad ist auf echter Hardware noch nicht bestätigt.** Beim Test war kein anderer
  Verkehr am Bus. Er ist nur durch Host-Tests abgedeckt.
- Die API kann sich bis 1.0 noch ändern.

## Links

- Repo: https://github.com/konnextra/KonnextraKNX
- PlatformIO: https://registry.platformio.org/libraries/konnextra/KonnextraKNX
- Doku / Release Notes: https://konnextra.at/docs/?page=releasenotes&lang=de
- API-Referenz: https://konnextra.github.io/KonnextraKNX/annotated.html
- Registry-PR: https://github.com/arduino/library-registry/pull/9305
