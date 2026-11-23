# 0004 — Zustandsloses Empfangen: `subscribe` + `onTelegram`

Datum: 24.11.2026 (Entscheidung Florian: Variante „B + A darauf“, Event-Callback wie `bind()`,
Name `subscribe`, mehr als 16 Plätze).

## Kontext
Senden geht ohne Objekt (`knx.send(ga, Dpt1(true))`), Empfangen nur über ein globales
`KnxObject`. LostWolf (knx-user-forum, #11) will ohne Objekt zuhören; die Tasmota-Schicht
abonniert zur Laufzeit aus Berry und braucht Kontext je Abo. Die Diplomarbeit hatte dafür
`knx.bind(ga, dpt, cb)`; das Redesign (PLAN §6/§7, `73e1a66`) hat es gestrichen, weil es „den
DPT leakt“ und der zustandslose Weg per Definition nicht empfängt.

## Entscheidung
- Diese Streichung wird zurückgenommen: Der DPT steht auch beim Senden da (`Dpt1(true)`) und ist
  beim Empfang nicht aus dem Bus ableitbar; der Ersatz `KnxObject` verlangt ein globales Objekt.
- `knx.onTelegram(cb[, ctx])`: ein Platz, jedes Gruppentelegramm inkl. Read und Fremdverkehr.
- `knx.subscribe(ga, dpt, cb[, ctx])` / `knx.unsubscribe(ga)`: nur Write/Response, `false` wenn voll.
- Größe per `knx.setSubscriptionSlotLimit(n)` (Gesamtzahl, Standard 64), nur vor dem ersten
  `subscribe()` wirksam. **Ausnahme von „keine dynamische Allokation“:** die Tabelle wird beim
  ersten `subscribe()` genau einmal per `malloc` angelegt und nie freigegeben oder vergrößert
  (kein Fragmentieren). Florian 24.11.: Array selbst anlegen und übergeben ist zu mühsam.
- Beide Callbacks bekommen ein `KnxTelegram` (Adresse, Absender, Typ, Wert), wahlweise mit `void* ctx`.
- Details: [`specs/2026-11-23-subscribe-design.md`](../specs/2026-11-23-subscribe-design.md).

## Konsequenzen
- Neuer öffentlicher Typ und neue Coordinator-API → `Changes.md`, Doxygen, neues Beispiel,
  Website-Examples en+de.
- Ohne `subscribe` 0 Byte; mit Standard 768 B (32-Bit) bzw. 512 B (AVR) Heap. `malloc`-Fehler → `subscribe` liefert `false`.
- Read-Anfragen lösen `subscribe` nie aus; das Beantworten von Reads bleibt ein eigenes Thema.
