# Kontext: Testaufbau und Werkzeuge

Stand 22.11.2026. Kontext, keine Entscheidung.

## Werkzeuge auf Florians Mac
- **PlatformIO ist nicht im PATH der Claude-Shell:** `~/.platformio/penv/bin/pio` benutzen
  (oder `export PATH=~/.platformio/penv/bin:$PATH`).
- **Doxygen:** `/opt/homebrew/bin` vorne in den PATH (siehe CLAUDE.md).
- **Python:** `python3` ist 3.9.6 — zu alt für xknx 4.x (braucht ≥ 3.11). Homebrew-Python
  `/opt/homebrew/bin/python3.12` nehmen, venv z.B. unter `extras/bench/.venv` (gitignoren).
- Shell ist **zsh**: unquotierte Variablen werden nicht wortgesplittet; Variablennamen wie
  `status` sind reserviert.

## Testkoffer
- KNX-Bus mit Spannungsversorgung, Taster, Aktor (Licht), IP-Schnittstelle. Der Koffer spannt
  einen eigenen Access Point auf.
- **Gewählter Netzaufbau (Option 1):** Mac per Ethernet-Kabel (USB-C-Adapter) an die
  IP-Schnittstelle bzw. den Koffer, **WLAN bleibt erste Dienstreihenfolge** fürs Internet —
  sonst bricht die Claude-Session ab. Am Ethernet-Adapter eine statische IP ohne Router.
- Florian kann zusätzlich mit der **ETS-Diagnose** (Gruppenmonitor/Busmonitor) zuschauen —
  einzige Quelle, die TP-ACKs zeigt, solange kein Busmonitor-Sketch existiert.
- **Sicherheit:** BCU nicht isoliert. Mac am Akku, kein geerdetes Netzteil/Gerät am XIAO,
  solange er am Bus hängt.

## Bench-Sketch
`examples/BenchTest/BenchTest.ino`, physikalische Adresse 1.1.5, `KNX_CHIP` per `#define`.
`KnxLight lamp(knx, "0/1/1", "1/1/1")` toggelt alle 5 s; `KnxPercent` hört auf 0/2/1.
Debug-Modus ist an (`KNX_VERBOSE true`).

Verdrahtung XIAO C6 ↔ BCU: XIAO D6 (TX) → BCU Pin 5 RXD; BCU Pin 6 TXD → XIAO D7 (RX); GND–GND.
BCU-Pins 1/2 (5 V/3V3) frei lassen, XIAO nur über USB versorgen.

```bash
PLATFORMIO_SRC_DIR=examples/BenchTest ~/.platformio/penv/bin/pio run --target upload
~/.platformio/penv/bin/pio device monitor   # 115200
```

## Test 1: Licht schalten
Erwartet im Log (Debug an):
- `DRV ncn5130 sysstat 0xFB: v20v ok vdd2 ok vbus ok vfilt ok xtal ok, mode normal`
- `DRV ncn5130 address 1.1.5 confirmed, configure 0x21 (auto-ack on)`
- `DRV ncn5130 begin: ok, state 0x07`
- alle 5 s `TX -> GA 0/1/1` mit `DRV con byte 0x8B -> positive`; das Licht schaltet.

Bei Fehler nennt das Log den Schritt (`FAILED at reset / system status / address
confirmation / state`) — gegen die Liste „Ungeprüft“ in `ncn5130.md` halten.

## Test 2: Empfang + Mitschnitt
Ohne Ethernet-Kabel: Mac ins WLAN des Testkoffers (kein Internet → Claude-Session läuft nicht
mit, Auswertung danach). Venv ist schon angelegt:
```bash
extras/bench/.venv/bin/python extras/bench/knx_listen.py            # Auto-Discovery, Ctrl-C beendet
extras/bench/.venv/bin/python extras/bench/knx_listen.py --gateway <IP> --minutes 20
```
Log landet in `extras/bench/logs/knx-<datum>.jsonl` (gitignored), eine Zeile je Telegramm
(Zeit, Richtung, Quelle, Ziel, Dienst, Rohwert). Parallel den seriellen XIAO-Log in eine Datei:
```bash
~/.platformio/penv/bin/pio device monitor | tee extras/bench/logs/xiao-$(date +%Y%m%d-%H%M%S).log
```
Während beides läuft: BenchTest toggelt 0/1/1; Taster mehrmals drücken; Zeitpunkte grob notieren.

## Test 2 und weiter
Zweites Board (1.1.6) an den Bus, normale Schalttelegramme vom Taster ansehen, parallel xknx
über die IP-Schnittstelle und ETS-Diagnose. Benötigt von Florian: IP der Schnittstelle,
statische Mac-IP, GA + physikalische Adresse des Tasters. Volle Matrix H1–H14: Spec 5.6.

## xknx (Recherche 21.11.2026, Details in Spec 5.5.1)
- xknx 4.1.1, `ConnectionConfig(connection_type=TUNNELING, gateway_ip=…, local_ip=<Ethernet-IP>)`,
  `xknx.telegram_queue.register_telegram_received_cb(cb, match_for_outgoing=True)`.
- Schnellblick ohne Code: die mitgelieferte CLI (`xknx` Group-Monitor) — vorher mit
  `--help` prüfen, nicht aus dem Gedächtnis.
- **Sieht keine TP-ACKs und keine Wiederholungen**, kein Busmonitor-Tunnel. Raw cEMI über einen
  Hook auf `cemi_handler.handle_raw_cemi`.
- Tunnel-Slots sind begrenzt → immer `await xknx.stop()`.
- ACKs/Wiederholungen: Busmonitor-Sketch auf einem dritten NCN-Board (U_Busmon `0x05`), geplant.
