# FermentationSensor

Firmware für einen Fermentationssensor auf dem **Arduino Nano RP2040 Connect**. Das Gerät erfasst Temperaturen und Differenzdruck und arbeitet im Anwendungsnetz ausschließlich als Client. Fachliche Zuordnung und Bewertung (etwa BeerDataStore, `beerId`, Plato oder Gärstatus) sind nicht Bestandteil der Firmware.

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

Solange WLAN besteht, versucht `ServerClient` einen verlorenen WebSocket zunächst am aktuellen Runtime-Endpoint mit begrenztem exponentiellem Backoff wieder aufzubauen. Nach drei erfolglosen Zyklen gilt der Endpoint als möglicherweise veraltet und DNS-SD wird erneut ausgeführt. Ist kein Service verfügbar, bleibt die Mess- und Bedienlogik aktiv und Discovery wird nach einem Backoff wiederholt; Messwerte werden nicht offline gepuffert.

Bei WLAN-Verlust werden WebSocket und laufende Discovery gestoppt. Es gibt ohne WLAN keine Gateway-Zugriffe. Nach Wiederherstellung wird der gespeicherte Last-Known-Cache erneut zuerst versucht; ein bloßer WLAN-Ausfall löscht ihn nicht.

### WebSocket und Registrierung

Der `ServerClient` verwendet weiterhin `ArduinoHttpClient`/`WebSocketClient` und den TX-Puffer `WS_TX_BUFFER_SIZE=256`. Jeder erfolgreiche WebSocket-Connect startet eine neue Session, setzt den Registrierungszustand zurück und sendet:

```json
{"type":"REGISTER_SENSOR","deviceId":"<persistente UUID>","deviceName":"<Name>"}
```

Messdaten dürfen erst nach `{"type":"REGISTER_SENSOR_ACK"}` gesendet werden. Registration-Timeout, Größenprüfung, Sendefehlerbehandlung und Reconnect-Backoff bleiben aktiv.

## Geräteidentität und persistente Daten

Beim ersten Start wird eine UUID v4 erzeugt, mit Hardwaredaten des NINA-W102 angereichert und zusammen mit einem editierbaren `deviceName` persistent gespeichert. Eine gültige UUID bleibt dauerhaft erhalten. Zusätzlich werden gespeichert:

- WLAN-SSID und -Passwort,
- der Last-Known-Gateway-Cache,
- die Zuordnung der DS18B20-ROM-IDs zu Bier- und Umgebungssensor.

## WLAN-Provisioning

Ohne gespeicherte WLAN-Konfiguration startet das Gerät den Access Point `FERM-01-Setup`. Die lokale Setup-Seite überträgt SSID und Passwort; dies ist ausschließlich WLAN-Provisioning. Nach erfolgreicher Provisionierung wird kein Discovery-, Bootstrap- oder HTTP-Anwendungsserver betrieben.

## Messungen und Bedienung

Zwei DS18B20-Sensoren messen Bier- und Umgebungstemperatur asynchron. Die Firmware erkennt fehlende bzw. wieder verbundene Sensoren zur Laufzeit. Die technische Differenzdruckverarbeitung und das bestehende Measurement-Format bleiben unverändert. Der Taster an D3 steuert die lokale Session:

```text
IDLE -> RUNNING -> PAUSED -> RUNNING
```

Die beiden Temperaturen werden weiterhin ungefähr alle 60 Sekunden frisch gemessen und lokal aktualisiert. Nur in `RUNNING` werden sie an ein registriertes Gateway übertragen. Die erste gültige gemeinsame Messung wird gesendet; danach erfolgt eine Übertragung erst, wenn sich mindestens eine Temperatur um mindestens 1,0 °C gegenüber ihrem zuletzt **erfolgreich** gesendeten Wert geändert hat. Fehlgeschlagene Sendungen verschieben diesen Vergleichswert nicht und werden deshalb beim nächsten gültigen Messzyklus erneut versucht. Nach jeder neuen erfolgreichen `REGISTER_SENSOR_ACK`-Session wird der nächste aktuelle gültige Temperaturstand unabhängig von der Differenz einmal übertragen; Pause/Resume allein erzwingt keine Übertragung. `pressurePa` kann dabei weiterhin als technischer Snapshot mitlaufen und beeinflusst die Temperatur-Sendeentscheidung nicht.

Die Gateway-Discovery-/Reconnect-Infrastruktur und die persistente Geräteidentität wurden geprüft: WLAN-Reconnect, Cache-first Gateway-Auswahl, DNS-SD-Fallback, Speichern entdeckter Endpunkte, WebSocket-Backoff, Rediscovery und Registrierung pro Verbindung sind vorhanden. `deviceId` und `deviceName` liegen persistent im Flash; eine neue UUID entsteht nur bei fehlender oder ungültiger Konfiguration.

**Offline measurement buffering is not implemented.** Es gibt weder RAM-/Ring-/Flash-Queue noch Replay gespeicherter Messwerte nach einem Reconnect.

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

Ein Ereignis wird dem Fenster zugeordnet, in dem der Detektor es beim Release als gültig abschließt. Die Fenstergrenze wird vor der Ereigniszuordnung verarbeitet; ein exakt auf der Grenze abgeschlossenes Ereignis zählt deshalb ausschließlich zum neuen Fenster. Der Sensor hält nur das laufende und einen abgeschlossenen Datensatz (`startedAtMs`, `durationMs`, `bubbleCount`). `completedWindow()` darf diesen Datensatz beliebig oft lesen, ohne ihn zu konsumieren; erst `acknowledgeCompletedWindow()` bestätigt die erfolgreiche Verarbeitung. Die serielle Diagnose liest nur und bestätigt nicht. Wird der einzelne Ergebnis-Slot vor einem weiteren Abschluss nicht bestätigt, ersetzt das neueste Ergebnis deterministisch das ältere („latest completed window wins“); es gibt weder eine wachsende Historie noch einen Offline-Puffer. Bei aktivierter Druckdiagnose wird jeder neue Abschluss einmal ausgegeben, einschließlich Null-Fenstern:

```text
BUBBLE_WINDOW,<startMs>,<durationMs>,<bubbleCount>
```

`BubbleActivityWindow` und Bubble Detection sind ausschließlich technische Messdaten. Es werden derzeit weder BubbleEvents noch Activity Windows an das Gateway übertragen. Der Sensor bewertet weder Gäraktivität noch Gärfortschritt.

### Architekturgrenzen der Bubble-Aktivität

- **Sensor:** misst Druck, kalibriert die technische Erkennung, erkennt `BubbleEvent`s und aggregiert deren technische Aktivität.
- **Gateway:** übernimmt später ausschließlich Transport beziehungsweise Übersetzung dieser Daten.
- **BeerDataStore / Backend:** übernimmt später Speicherung und fachliche Auswertung.
- **UI:** übernimmt später die Anzeige.

Der Sensor erzeugt insbesondere keine Aussagen wie „Gärung stark“, „Gärung schwach“, „Gärung fast beendet“ oder „Gärung beendet“. Solche Hinweise dürfen später ausschließlich im Backend abgeleitet werden. Plato-Auswertung, Gateway-Übertragung der Aktivitätsfenster und Offline-Pufferung bleiben bewusst außerhalb dieser Firmware-Erweiterung.

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

Nicht implementiert sind Offline Queue/Replay, fachliche Bubble-Auswertung, Plato-, Alkohol- oder Vergärungsberechnung sowie eine Bewertung des Gärverlaufs. Der Sensor kennt weder `beerId` noch ein Datenbankmodell oder einen fachlichen Gärstatus.
