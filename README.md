# FermentationSensor

Firmware für einen Fermentationssensor auf dem **Arduino Nano RP2040 Connect**. Das Gerät erfasst Temperaturen und Differenzdruck und überträgt Messwerte an den BeerDataStore.

## Aktueller Funktionsumfang

### Geräteidentität und persistente Konfiguration

Beim ersten Start wird eine UUID v4 erzeugt, mit Hardwaredaten des NINA-W102 angereichert und zusammen mit einem editierbaren `deviceName` persistent gespeichert. Eine bereits gespeicherte gültige UUID bleibt unverändert. Ebenfalls persistent gespeichert werden:

- WLAN-SSID und -Passwort,
- Host, Port und WebSocket-Pfad des Backends,
- die Zuordnung der DS18B20-ROM-IDs zu Bier- und Umgebungssensor.

### WLAN-Provisioning

Ohne gespeicherte WLAN-Konfiguration startet das Gerät den Access Point `FERM-01-Setup`. Die lokale Setup-Seite überträgt SSID und Passwort per `POST` und `application/x-www-form-urlencoded`; das Passwort wird nicht geloggt. Mit gespeicherten Zugangsdaten verbindet sich der `NetworkManager` automatisch und verwendet bei Ausfällen einen Reconnect-Backoff.

### Discovery und HTTP-Bootstrap

Im WLAN beantwortet das Gerät `DISCOVER_FERMENTATION_SENSORS` auf UDP-Port 4210 mit UUID und Gerätename. Über `POST /connect` kann anschließend eine JSON-Konfiguration mit `host`, `port` und `path` gesetzt werden. Eine gültige neue Konfiguration wird sofort übernommen: Die bestehende Backend-Verbindung wird getrennt, mit dem neuen Ziel wieder aufgebaut und der Sensor erneut registriert.

### WebSocket und Registrierung

Der `ServerClient` verbindet sich mit dem konfigurierten WebSocket-Endpunkt und sendet `REGISTER_SENSOR`. Messdaten werden erst nach `REGISTER_SENSOR_ACK` übertragen. Verbindungs- und Registrierungsfehler führen zu einem erneuten Verbindungsversuch mit begrenztem exponentiellem Backoff. Ohne WLAN erfolgen keine Backend-Verbindungsversuche.

### Messungen und MeasurementSession

Zwei DS18B20-Sensoren messen Bier- und Umgebungstemperatur asynchron. Die Firmware erkennt fehlende bzw. wieder verbundene Sensoren zur Laufzeit und sendet keine ungültigen Temperaturpaare. Der Differenzdrucksensor wird in einer laufenden Session aktualisiert; die aktuelle Implementierung arbeitet noch im Simulationsmodus.

Der Taster an D3 steuert die lokale `MeasurementSession`:

```text
IDLE -> RUNNING -> PAUSED -> RUNNING
```

Nur in `RUNNING` werden frische Temperaturmessungen an einen registrierten Server gesendet und Druckmessungen aktualisiert. In `PAUSED` werden keine Messdaten gesendet. Ein Sensorfehler beendet eine bereits laufende Session nicht, verhindert aber ungültige Übertragungen.

### LEDs

| LED | Pin | Bedeutung |
|---|---:|---|
| Status | D2 | Betriebs-/Pausezustand |
| Fehler | D5 | Sensor-, WLAN- oder Backendfehler mit unterschiedlichen Blinkintervallen |
| Session | D6 | aktive MeasurementSession |

Fehlerpriorität: **Sensor > WLAN/Netzwerk > Backend (nicht verbunden oder nicht registriert) > kein Fehler**.

## Ablauf

```text
Start
  -> WLAN (oder Provisioning)
  -> UDP Discovery
  -> HTTP Bootstrap
  -> WebSocket
  -> REGISTER_SENSOR
  -> REGISTER_SENSOR_ACK
  -> RUNNING
  -> Temperatur + Differenzdruck
```

## Hardware

- Arduino Nano RP2040 Connect (RP2040 + NINA-W102)
- zwei DS18B20 an D4 mit gemeinsamem 4,7-kΩ-Pull-up
- Measurement-Taster an D3 (`INPUT_PULLUP`)
- Status-LED D2, Fehler-LED D5, Session-LED D6
- vorgesehen: DFRobot SEN0343 / Fermion LWLP5000 ±500 Pa über I²C

## Build

Das Projekt verwendet PlatformIO:

```sh
pio run
```

Die wesentlichen Bibliotheken sind WiFiNINA, ArduinoHttpClient, OneWire und DallasTemperature.

## Noch offen

Folgende Funktionen sind ausdrücklich noch nicht implementiert:

- Offline Queue,
- Bubble-Erkennung,
- Berechnung von Bubbles/min,
- Druck-Peak- und Druckverlaufsanalyse,
- Bewertung des Fermentationsverlaufs.

Ebenfalls nicht Teil des aktuellen Stands sind das Runden der Temperaturen auf ganze Grad, die Übertragung nur bei Temperaturänderung und ein zusätzliches Maximalintervall.
