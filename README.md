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

Nur in `RUNNING` werden frische Messwerte an ein registriertes Gateway gesendet. Es gibt derzeit bewusst keine Offline Queue und keinen Replay.

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

Nicht implementiert sind Offline Queue/Replay, Bubble-Auswertung, Plato-, Alkohol- oder Vergärungsberechnung sowie eine Bewertung des Gärverlaufs. Der Sensor kennt weder `beerId` noch ein Datenbankmodell oder einen fachlichen Gärstatus.
