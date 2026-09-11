# FermentationSensor

Firmware für einen Fermentationssensor auf dem **Arduino Nano RP2040 Connect**. Das Gerät erfasst Temperaturen und Differenzdruck und arbeitet im Anwendungsnetz ausschließlich als Client. Fachliche Zuordnung und Bewertung (etwa BeerDataStore, `beerId`, Plato oder Gärstatus) sind nicht Bestandteil der Firmware.

## Getestete Build-Umgebung

Die derzeit getestete und in `platformio.ini` fixierte Umgebung ist:

- PlatformIO-Plattform `platformio/raspberrypi` **1.11.0**,
- Framework `framework-arduino-mbed` **4.0.10**,
- Board `nanorp2040connect` (Arduino Nano RP2040 Connect).

Ein Upgrade der Plattform oder des Frameworks ist ausdrücklich ein separater
Schritt und darf nicht implizit zusammen mit Änderungen am persistenten Speicher
erfolgen.

## Netzwerkarchitektur

Nach dem WLAN-Provisioning betreibt der Sensor keinen anwendungsspezifischen TCP- oder UDP-Server. Der Ablauf ist:

```text
WiFi
  -> Last-Known Gateway versuchen
  -> falls nötig DNS-SD `_brewferment._tcp.local`
  -> Host auf IPv4-Adresse auflösen und TXT/SRV-Daten validieren
  -> Endpoint zur Laufzeit bestimmen
  -> Endpoint als Cache speichern
  -> WebSocket
  -> REGISTER_SENSOR
  -> REGISTER_SENSOR_ACK
  -> Measurements
```

Das Gateway veröffentlicht den DNS-SD-Service `_brewferment._tcp.local` mit Host und Port sowie den TXT-Einträgen:

```text
path=/fermentation/socket
version=1
```

Die Firmware fragt DNS-SD direkt über den von WiFiNINA bereitgestellten Multicast-UDP-Transport ab. Dadurch bleibt die Lösung mit dem NINA-W102 des Nano RP2040 Connect kompatibel und benötigt keine zusätzliche, für andere Netzwerk-Stacks entwickelte mDNS-Bibliothek. PTR-, SRV-, TXT- und A-Records werden ausgewertet; nur eine IPv4-Adresse, ein gültiger Port, ein absoluter WebSocket-Pfad und die derzeit unterstützte Protokollversion `1` werden akzeptiert.

### Last-Known-Gateway-Cache

IPv4-Adresse, Port, WebSocket-Pfad und Protokollversion des zuletzt entdeckten Gateways werden über die bestehende `FlashStorage`-Abstraktion gespeichert. **Dieser Endpoint ist ausschließlich ein Cache, keine feste Serverkonfiguration.**

Nach jeder neuen WLAN-Verbindung wird ein gültiger Cache zuerst probiert. Funktionieren WebSocket-Verbindung und Registrierung, findet keine unnötige Discovery statt. Schlägt der erste Startversuch gegen den Cache fehl, beginnt DNS-SD. Ein erfolgreicher Discovery-Fund ersetzt den Cache.

### Reconnect und Rediscovery

Solange WLAN besteht, versucht `ServerClient` einen verlorenen WebSocket zunächst am aktuellen Runtime-Endpoint mit begrenztem exponentiellem Backoff wieder aufzubauen. Nach drei erfolglosen Zyklen gilt der Endpoint als möglicherweise veraltet und DNS-SD wird erneut ausgeführt. Ist kein Service verfügbar, bleibt die Mess- und Bedienlogik aktiv und Discovery wird nach einem Backoff wiederholt; übertragungswürdige Messwerte werden bis zur Kapazitätsgrenze der RAM-Outbox gepuffert.

Bei WLAN-Verlust werden WebSocket und laufende Discovery gestoppt. Es gibt ohne WLAN keine Gateway-Zugriffe. Nach Wiederherstellung wird der gespeicherte Last-Known-Cache erneut zuerst versucht; ein bloßer WLAN-Ausfall löscht ihn nicht.

### WebSocket und Registrierung

Der `ServerClient` verwendet weiterhin `ArduinoHttpClient`/`WebSocketClient` und den TX-Puffer `WS_TX_BUFFER_SIZE=256`. Jeder erfolgreiche WebSocket-Connect startet eine neue Session, setzt den Registrierungszustand zurück und sendet:

```json
{"type":"REGISTER_SENSOR","deviceId":"<persistente UUID>","deviceName":"<Name>"}
```

Messdaten dürfen erst nach `{"type":"REGISTER_SENSOR_ACK"}` gesendet werden. Sie können jedoch bereits vorher in die gemeinsame Measurement-Outbox eingestellt werden. Registration-Timeout, Größenprüfung, Sendefehlerbehandlung und Reconnect-Backoff bleiben aktiv.

Die beiden gepufferten Sensor-Nachrichten werden vollständig so übertragen (das Druckfeld der Temperatur ist optional):

```json
{"type":"TEMPERATURE_MEASUREMENT","deviceId":"...","sequence":100,"beerTemperature":20.4,"ambientTemperature":18.7,"pressurePa":0.08,"measurementAgeSeconds":120}
{"type":"BUBBLE_ACTIVITY","deviceId":"...","sequence":101,"bubbleCount":8,"windowSeconds":60,"windowEndAgeSeconds":185}
```

Abgeschlossene technische Aktivitätsfenster werden unabhängig von der Temperatur-Sendeschwelle übertragen. Das gilt ausdrücklich auch für Fenster mit null erkannten Blubbs.

Alle Messungstypen erhalten beim Einstellen in die Outbox eine gemeinsame Sequenznummer. Das Gateway bestätigt die jeweils älteste akzeptierte Nachricht mit:

```json
{"type":"MEASUREMENT_ACK","sequence":1}
```

Erst dieses ACK mit exakt zur FIFO-Spitze passender `sequence` bestätigt die Übertragung; ein erfolgreicher lokaler WebSocket-Write genügt nicht. Bleibt das ACK länger als fünf Sekunden aus, wird dieselbe Nachricht ohne blockierendes Warten und mit derselben Sequenznummer erneut gesendet. Auch nach einem WebSocket-Abbruch bleibt sie erhalten und wird nach Reconnect sowie erneutem `REGISTER_SENSOR_ACK` wiederholt. Falsche, veraltete oder vorgreifende ACK-Sequenzen werden ignoriert. Der frühere `BUBBLE_ACTIVITY_ACK` wird nicht mehr akzeptiert.

## Geräteidentität und persistente Daten

Beim ersten Start wird eine UUID v4 erzeugt, mit Hardwaredaten des NINA-W102 angereichert und zusammen mit einem editierbaren `deviceName` persistent gespeichert. Eine gültige UUID bleibt dauerhaft erhalten. Zusätzlich werden gespeichert:

- WLAN-SSID und -Passwort,
- der Last-Known-Gateway-Cache,
- die Zuordnung der DS18B20-ROM-IDs zu Bier- und Umgebungssensor,
- der Start des nächsten freien Measurement-Sequence-Blocks.

Die aktuellen KV-Keys unter `/kv/` sind `device_config`, `wifi_config`,
`temperature_config`, `gateway_cache` und `measurement_sequence_v1`. Aus älteren
Firmwareständen können außerdem `wifi_ssid`, `wifi_password`, `device_uuid`,
`device_name` und `server_config` vorhanden sein. Diese Legacy-Keys werden nicht
automatisch entfernt.

Beim Boot listet die Flash-Diagnose ausschließlich Keyname, Nutzdatengröße und
Flags als `KV_ENTRY,<key>,<size>,<flags>` auf. Danach folgen `KV_ENTRY_COUNT` und
`KV_LIVE_DATA_BYTES`. Werte – insbesondere WLAN-Zugangsdaten – werden dabei nie
ausgegeben. Diese Live-Daten-Summe beschreibt nicht den durch alte TDBStore-
Logeinträge belegten Rohspeicher; der Vergleich mit der KV-Gesamtgröße hilft aber,
zwischen vielen aktiven Keys und Log-/Compaction-Druck zu unterscheiden.

### Persistente Measurement-Sequenzen

Die stabile `deviceId` wird mit logisch einmaligen Measurement-Sequenzen kombiniert. Dazu reserviert der Sensor Sequenzblöcke mit **65.536** Werten über den versionierten KV-Key `measurement_sequence_v1`. Bevor auch nur die erste Sequenz eines Blocks verwendet wird, schreibt die Firmware bereits den Start des darauffolgenden Blocks in den Flash (**reserve before use**). Im Normalbetrieb entsteht daher ein Flash-Write pro 65.536 Messungen und nicht pro Messung.

Die erste Initialisierung beginnt absichtlich bei `0x01000000` (16.777.216). Dieser deutlich höhere Migrationsbereich verhindert auf bereits eingesetzten Geräten eine Kollision mit niedrigen Sequenzen, die von der früheren flüchtigen Zählung schon an BeerDataStore übertragen worden sein können. Bei jedem Neustart wird der nächste Block reserviert. Nicht verwendete Werte des vorherigen Blocks werden dabei bewusst übersprungen; das ist unschädlich und garantiert, dass eine möglicherweise schon verwendete Sequenz nicht erneut vergeben wird. Auch ein Stromausfall unmittelbar nach einer erfolgreichen Reservierung kann daher höchstens einen unbenutzten Block überspringen.

Die Outbox selbst bleibt ausschließlich im RAM. Ein Neustart verliert somit weiterhin unbestätigte Messwerte, deren kompletter reservierter Sequence-Bereich wird danach aber nicht wiederverwendet. Schlägt die Flash-Reservierung fehl oder ist der gespeicherte Zustand ungültig, arbeitet die Vergabe geschlossen: Es gelangt keine Messung mit einer unsicheren Sequenz in die Outbox, und `MEASUREMENT_SEQUENCE_RESERVATION_FAILED` wird seriell gemeldet. Eine Erschöpfung des sicheren `uint32_t`-Raums führt ebenfalls nicht zu Wrap-around oder Recycling, sondern stoppt weitere Vergaben.

### Explizite Storage-Maintenance

Ein durch obsolete Log-Records gefüllter TDBStore kann ausschließlich auf
ausdrücklichen Benutzerwunsch kompakt neu aufgebaut werden. Dazu wird während
der dreisekündigen seriellen Startphase die Zeile `COMPACT_STORAGE` gesendet.
Sensoren, Messsession und Netzwerk sind dann noch nicht gestartet. Ein normaler
Boot führt diese Wartung **niemals** aus.

Vor `kv_reset("/kv/")` liest und validiert die Wartung `device_config`, den
nächsten sicheren Block aus `measurement_sequence_v1` sowie alle vorhandenen
Records `wifi_config`, `temperature_config` und `gateway_cache` vollständig in
getrennte RAM-Strukturen. Ein fehlender Pflichtrecord oder ein ungültiger bzw.
nicht lesbarer vorhandener Record bricht vor dem Reset ab. Der interne
TDBStore-Key `/kv/TDBS` ist kein Anwendungszustand und wird weder gesichert noch
wiederhergestellt. Nach Reset und Restore werden Größe, Semantik und sämtliche
Bytes erneut geprüft. Erst danach initialisiert der normale Allocator und
reserviert den gesicherten nächsten freien 65.536er-Block; eine Sequenz wird
weder auf den Initialwert zurückgesetzt noch erneut verwendet. Jeder Fehler ist
fail-closed und verhindert den Messbetrieb.

**Stromausfallwarnung:** Backup in RAM, `kv_reset()` und Restore sind ohne einen
zweiten persistenten Speicherbereich nicht vollständig transaktions- oder
stromausfallsicher. Ein Stromverlust nach dem Reset kann Identität,
WLAN-Konfiguration oder Sequence-State zerstören. Die Wartung darf deshalb nur
bewusst in einem Wartungsfenster mit stabiler Versorgung ausgeführt werden. Die
Diagnose meldet die jeweilige Phase, aber niemals UUID, SSID oder Passwort.

Die APIs bleiben bewusst getrennt: `resetRuntimeMeasurementState()` leert nur
flüchtige Outbox-/Senderegel-Zustände, `compactPersistentStorage()` baut den
KVStore unter Erhalt des Anwendungszustands neu auf, und `factoryReset()` löscht
den persistenten Namespace. Compaction ersetzt keinen Factory Reset.

## WLAN-Provisioning

Ohne gespeicherte WLAN-Konfiguration startet das Gerät den Access Point `FERM-01-Setup`. Die lokale Setup-Seite überträgt SSID und Passwort; dies ist ausschließlich WLAN-Provisioning. Nach erfolgreicher Provisionierung wird kein Discovery-, Bootstrap- oder HTTP-Anwendungsserver betrieben.

## Messungen und Bedienung

Zwei DS18B20-Sensoren messen Bier- und Umgebungstemperatur asynchron. Die Firmware erkennt fehlende bzw. wieder verbundene Sensoren zur Laufzeit. Die technische Differenzdruckverarbeitung und das bestehende Measurement-Format bleiben unverändert. Der Taster an D3 steuert die lokale Session:

```text
IDLE -> RUNNING -> PAUSED -> RUNNING
```

Die beiden Temperaturen werden weiterhin ungefähr alle 60 Sekunden frisch gemessen und lokal aktualisiert. Nur in `RUNNING` prüft die Senderegel, ob eine Messung übertragungswürdig ist. Die erste gültige gemeinsame Messung wird eingestellt; danach erst wieder, wenn sich mindestens eine Temperatur um mindestens 1,0 °C gegenüber ihrem zuletzt **erfolgreich eingestellten** Wert geändert hat. Kleine Änderungen summieren sich damit gegen die letzte von der Outbox übernommene Temperatur; dieselbe relevante Messung wird offline nicht jede Minute erneut eingestellt. Nach jeder neuen erfolgreichen `REGISTER_SENSOR_ACK`-Session wird der nächste aktuelle gültige Temperaturstand unabhängig von der Differenz über die Outbox aufgenommen. Entspricht er bereits dem letzten Outbox-Eintrag vom Typ Temperatur, wird kein Duplikat angehängt. Pause/Resume allein erzwingt keinen Snapshot. `pressurePa` wird zum Messzeitpunkt optional mitkopiert und beeinflusst die Senderegel nicht.

Die Gateway-Discovery-/Reconnect-Infrastruktur und die persistente Geräteidentität wurden geprüft: WLAN-Reconnect, Cache-first Gateway-Auswahl, DNS-SD-Fallback, Speichern entdeckter Endpunkte, WebSocket-Backoff, Rediscovery und Registrierung pro Verbindung sind vorhanden. `deviceId` und `deviceName` liegen persistent im Flash; eine neue UUID entsteht nur bei fehlender oder ungültiger Konfiguration.

## Generic Measurement Outbox

Übertragungswürdige Temperaturmessungen und abgeschlossene Bubble-Activity-Fenster laufen in zeitlicher Reihenfolge durch denselben statisch reservierten Ringbuffer. Beide Typen teilen den persistent abgesicherten `uint32_t`-Sequenzraum; ein Wrap-around ist ausdrücklich nicht erlaubt. Die Queue enthält strukturierte Payloads und keine vorbereiteten JSON-Strings. Rohdruckreihen, einzelne BubbleEvents, Baseline, Noise, Trigger-, Kalibrierungs- und Diagnosedaten sowie Registrierungsmeldungen werden nicht gepuffert.

Die Kapazität beträgt 384 Einträge. Ein `OutboxEntry` belegt auf der Zielplattform 28 Byte, die Einträge reservieren somit 10.752 Byte (10,5 KiB) RAM zuzüglich weniger Verwaltungsbytes. Bei überwiegend einem Bubble-Fenster pro Minute reichen 360 Plätze grob für sechs Stunden; zusätzliche Temperaturereignisse reduzieren die effektive Offline-Dauer. Das ist ausdrücklich keine feste Sechs-Stunden-Garantie.

Ausschließlich der älteste Eintrag wird gesendet und bleibt bis zum passenden `MEASUREMENT_ACK` in-flight. Bei jedem Sende- und Retry-Versuch entstehen `measurementAgeSeconds` beziehungsweise `windowEndAgeSeconds` neu aus der vorzeichenlosen Differenz `millis() - capturedAtMs`; dadurch funktionieren die Altersangaben auch über einen `millis()`-Wrap und benötigen weder RTC noch erfundene UTC-Zeitstempel. Bereits gepufferte Daten werden auch in `PAUSED` weiter gesendet und bestätigt, obwohl dort keine neue Druckmessung oder Bubble-Aggregation stattfindet.

Ist der Ringbuffer voll, gilt **DROP OLDEST**: Der älteste Eintrag wird verworfen, ein eventueller In-flight-Zustand zurückgesetzt, der neue Eintrag angehängt und der zentrale Drop-Zähler erhöht. Seine Sequenz bleibt dauerhaft verbraucht. Ein verspätetes ACK des verworfenen Eintrags entfernt keine weiteren Daten. Diese Strategie bedeutet bei langen Ausfällen bewussten Datenverlust zugunsten aktuellerer Messungen. Die Outbox liegt ausschließlich im RAM; ein Neustart verliert alle noch nicht bestätigten Einträge.

### Druckkalibrierung und technische Blubb-Erkennung

Der DFRobot LWLP5000 / SEN0343 misst den Differenzdruck zwischen Gärbehälter und Umgebung in Pascal. Während einer laufenden `RUNNING`-Session liest die Firmware den Sensor nicht blockierend alle 100 ms (etwa 10 Messwerte pro Sekunde) und gibt bei aktiviertem `PRESSURE_DIAGNOSTICS_ENABLED` eine maschinenlesbare Zeile aus:

```text
PRESSURE,<millis>,<pressurePa>
```

Der Zeitstempel basiert auf `millis()`, der Druckwert besitzt zwei Nachkommastellen. Mit `PRESSURE_DIAGNOSTICS_ENABLED = false` wird nur diese serielle Rohdatenausgabe abgeschaltet; die interne Druckmessung und der optionale `pressurePa`-Snapshot einer tatsächlich gesendeten `TEMPERATURE_MEASUREMENT` bleiben erhalten. Die 10-Hz-Rohwerte erzeugen keine zusätzlichen Gateway-Nachrichten. In `IDLE` und `PAUSED` findet weiterhin keine laufende Druckmessreihe statt.

Beim ersten Wechsel nach `RUNNING` seit einem Neustart kalibriert sich die Druckauswertung fünf Minuten lang. Während dieser Phase werden keine Blubbs gezählt. Je 50 Rohwerte (etwa fünf Sekunden) werden zu einem Block zusammengefasst. Der Median von höchstens 60 Blockmittelwerten bildet die robuste Baseline; der Median der Standardabweichungen innerhalb der Blöcke beschreibt das typische Rauschen. Damit beeinflussen einzelne Ausreißer höchstens einen Block und nicht unmittelbar das Ergebnis. Statt rund 3000 Rohwerten reserviert die Implementierung zwei feste Arrays mit je 64 `float`-Werten (zusammen 512 Byte) als begrenzten Kalibrierungs-Arbeitsspeicher. Nach Abschluss werden deren temporäre Inhalte gelöscht und nicht mehr ausgewertet; als gültige Daten bleiben nur Baseline, Rauschen, Trigger-/Release-Schwelle, Zustandsdaten, Zähler und das letzte Event. Eine Kalibrierung wird nicht im Flash gespeichert.

Der dynamische Trigger ist `max(0,50 Pa, noisePa * 5,0)`, die Release-Schwelle beträgt `triggerDeltaPa * 0,4`. Außerhalb aktiver Blubbs und der Sperrzeit folgt die Baseline langfristigem Drift mit `alpha = 0,001`. Ein positiver Peak startet ein Event am Trigger und beendet es beim Release. Ereignisse unter 100 ms werden verworfen; Ereignisse über 3000 ms werden abgebrochen. Anschließend verhindert eine Sperrzeit von 500 ms eine Mehrfachzählung durch Nachschwingen. Bei Pause wird ein halbfertiges Ereignis verworfen. Eine vorhandene Kalibrierung bleibt bei Resume erhalten; auch eine pausierte Kalibrierung wird mit ihrer verbleibenden aktiven Laufzeit fortgesetzt.

Die genannten Werte sind zentral in `Config.h` konfigurierte **Startwerte**, die nach Messungen realer Druckkurven angepasst werden können, ohne die Erkennungslogik zu ändern. Bei aktivierter Druckdiagnose erscheinen zusätzlich genau einmal pro Kalibrierung und einmal pro gültigem technischen Ereignis:

```text
PRESSURE_CALIBRATED,baseline=0.01,noise=0.08,trigger=0.50,release=0.20
BUBBLE,<startMs>,<durationMs>,<peakDeltaPa>
```

Direkt nach erfolgreicher Kalibrierung startet außerdem ein technisches Bubble-Aktivitätsfenster. Alle gültigen `BubbleEvent`s werden in festen 60-Sekunden-Fenstern gezählt. Die Fensterdauer basiert ausschließlich auf aktiver `RUNNING`-Zeit: Während `PAUSED` stehen Zeit und Zähler, bei Resume läuft dasselbe Fenster mit seiner verbleibenden aktiven Zeit weiter. Auch Fenster ohne erkanntes Ereignis werden mit `bubbleCount = 0` abgeschlossen. Direkt nach jedem Abschluss startet das nächste Fenster.

Ein Ereignis wird dem Fenster zugeordnet, in dem der Detektor es beim Release als gültig abschließt. Die Fenstergrenze wird vor der Ereigniszuordnung verarbeitet; ein exakt auf der Grenze abgeschlossenes Ereignis zählt deshalb ausschließlich zum neuen Fenster. Dasselbe bereits gelesene Drucksample wird zusätzlich als Differenz zur bei der Kalibrierung festgestellten Baseline in das aktive Fenster aufgenommen. Aus Summe und Anzahl dieser Deltas entsteht beim Fensterabschluss `averagePressureDeltaPa`; die langsame Baseline-Nachführung der Bubble-Erkennung verändert diese feste Kalibrierungsreferenz nicht.

`bubbleCount` und `averagePressureDeltaPa` besitzen damit exakt dieselbe Zeitbasis. Nur aktive `RUNNING`-Zeit und die in dieser Zeit vorhandenen Samples gehen ein; `PAUSED` fügt weder Fensterzeit noch Drucksamples hinzu und Summe sowie Anzahl bleiben für Resume erhalten. Es wird keine Rohdruckhistorie oder dynamische Sample-Liste gespeichert, sondern ausschließlich laufende Summe und Anzahl. Ebenso werden keine Min-/Max-Werte ermittelt. Die Kalibrierungsphase erzeugt weiterhin keine Activity Windows. Sollte ein Fenster wider Erwarten kein gültiges Drucksample enthalten, gilt sein Mittelwert als nicht verfügbar und das Fenster wird nicht als gemessener 0-Pa-Wert in die Outbox übernommen.

Der Sensor hält nur das laufende und einen abgeschlossenen Datensatz (`startedAtMs`, `durationMs`, `completedAtMs`, `bubbleCount`, `averagePressureDeltaPa`, `pressureSampleCount`). `completedWindow()` darf diesen Datensatz beliebig oft lesen, ohne ihn zu konsumieren; erst nach erfolgreicher Kopie in die allgemeine Outbox bestätigt `acknowledgeCompletedWindow()` die Verarbeitung. Die serielle Diagnose liest nur und bestätigt nicht. Bei aktivierter Druckdiagnose wird jeder neue Abschluss einmal ausgegeben, einschließlich Null-Bubble-Fenstern:

```text
BUBBLE_WINDOW,<startMs>,<durationMs>,<bubbleCount>,<averagePressureDeltaPa>
```

`BUBBLE_ACTIVITY` enthält neben `bubbleCount` und `windowSeconds` nun `averagePressureDeltaPa`, den mittleren Differenzdruck relativ zur kalibrierten Baseline während desselben Fensters. Der Wert wird beim Enqueue vollständig kopiert und bleibt bei Retries unverändert; lediglich `windowEndAgeSeconds` wird beim Senden erneut aus dem Abschlusszeitpunkt berechnet.

`BubbleActivityWindow` und Bubble Detection sind ausschließlich technische Messdaten. An das Gateway werden nur abgeschlossene Activity Windows einschließlich Null-Bubble-Fenstern übertragen. Einzelne BubbleEvents, Rohdruck, Baseline, Noise, Triggerwerte, Min-/Max-Werte und Peaks werden nicht übertragen. Aus dem technischen Mittelwert wird im Sensor keine fachliche Gärungsinterpretation abgeleitet. Der Sensor bewertet weder Gäraktivität noch Gärfortschritt.

### Architekturgrenzen der Bubble-Aktivität

- **Sensor:** misst Druck, kalibriert die technische Erkennung, erkennt `BubbleEvent`s und aggregiert deren technische Aktivität.
- **Gateway:** bestätigt und übernimmt ausschließlich Transport beziehungsweise Übersetzung dieser Daten.
- **BeerDataStore / Backend:** übernimmt später Speicherung und fachliche Auswertung.
- **UI:** übernimmt später die Anzeige.

Der Sensor erzeugt insbesondere keine Aussagen wie „Gärung aktiv“, „Gärung stark“, „Gärung schwach“, „Gärung fast beendet“, „Gärung beendet“ oder „Plato stabil“. Solche Hinweise dürfen später ausschließlich im Backend abgeleitet werden. Fachliche Auswertung und Flash-Persistenz der Outbox bleiben bewusst außerhalb dieser Firmware-Erweiterung.

## LEDs

| LED | Pin | Bedeutung |
|---|---:|---|
| Status | D2 | Betriebs-/Pausezustand |
| Fehler | D5 | Sensor-, WLAN- oder Backendfehler mit unterschiedlichen Blinkintervallen |
| Session | D6 | aktive MeasurementSession |

Fehlerpriorität: **Sensor > WLAN/Netzwerk > Gateway (nicht verbunden oder nicht registriert) > kein Fehler**.

## Hardware und Build

- Arduino Nano RP2040 Connect (RP2040 + NINA-W102)
- zwei DS18B20 an D4 mit gemeinsamem 4,7-kΩ-Pull-up
- Measurement-Taster an D3 (`INPUT_PULLUP`)
- Status-LED D2, Fehler-LED D5, Session-LED D6
- vorgesehen: DFRobot SEN0343 / Fermion LWLP5000 ±500 Pa über I²C

```sh
pio run
```

Abhängigkeiten: WiFiNINA, ArduinoHttpClient, OneWire und DallasTemperature. DNS-SD nutzt die vorhandene WiFiNINA-`WiFiUDP`-API; es ist keine zusätzliche Library erforderlich.

## Bewusste fachliche Grenzen

Nicht implementiert sind flash-persistentes Offline-Replay, fachliche Bubble-Auswertung, Plato-, Alkohol- oder Vergärungsberechnung sowie eine Bewertung des Gärverlaufs. Der Sensor kennt weder `beerId` noch ein Datenbankmodell oder einen fachlichen Gärstatus.
