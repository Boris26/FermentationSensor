# FermentationSensor – Arduino Nano ESP32

Firmware für den **Arduino Nano ESP32** (ESP32-S3 / u-blox NORA-W106) mit Arduino Framework und PlatformIO. Dieses Branch-Target ist ausschließlich der Nano ESP32; weitere Boards werden nicht unterstützt.

## Build, Upload und Monitor

Die offizielle PlatformIO-Board-ID ist `arduino_nano_esp32`; das einzige Environment heißt `nanoesp32`.

```bash
pio run
pio run -e nanoesp32
pio run -e nanoesp32 -t upload
pio device monitor -b 115200
```

`ARDUINO_USB_MODE=1` und `ARDUINO_USB_CDC_ON_BOOT=1` aktivieren die native USB-CDC-Konsole. Upload und Monitor verwenden den USB-C-Anschluss des Boards.

## Pinbelegung

Die Belegung verwendet die standardmäßige **Arduino-Pinnummerierung** des Nano
ESP32. Die an `pinMode()` und `digitalWrite()` übergebenen Werte sind daher die
D-Pinnummern, nicht die rohen ESP32-GPIO-Nummern. In der ESP32-S3-Zuordnung des
Nano ESP32 sind dies:

| Funktion | Nano-Pin | ESP32-S3 GPIO | Hinweise |
|---|---:|---:|---|
| DS18B20 OneWire | D2 | GPIO5 | externer 4,7-kΩ Pull-up nach 3,3 V |
| Measurement/Pause Button | D3 | GPIO6 | Eingang mit internem Pull-up, Taster nach GND |
| Status LED | D4 | GPIO7 | externer Vorwiderstand |
| Sensor Error LED | D5 | GPIO8 | externer Vorwiderstand |
| Session LED | D6 | GPIO9 | externer Vorwiderstand |
| LWLP5000 SDA | A4/SDA | GPIO11 | Standard-`Wire`-Bus, 3,3 V |
| LWLP5000 SCL | A5/SCL | GPIO12 | Standard-`Wire`-Bus, 3,3 V |

Die Belegung meidet die nativen USB-Leitungen GPIO19/GPIO20 sowie die Strapping-Pins GPIO0/GPIO45/GPIO46. D2–D6 sind nicht mit den I2C-Pins A4/A5 belegt. Alle Signale sind ausschließlich 3,3-V-tolerant.

## Sensoren und Bedienung

Zwei DS18B20 werden asynchron ausgelesen und anhand ihrer persistenten IDs den Rollen `Ambient` und `Beer` zugeordnet. Der LWLP5000 läuft am Standard-I2C-Bus; Kalibrierung, Baseline, Rauschfilter, Bubble-Erkennung und Messfenster bleiben unverändert. Der Taster wechselt `RUNNING -> PAUSED -> RUNNING`. Der `StatusController` bleibt der zentrale Writer für die fachlichen LED-Zustände und stellt nach einem temporären Sensorfehler den vorherigen Zustand wieder her.

## WLAN und Gateway

Die Firmware nutzt den nativen ESP32-Stack aus `<WiFi.h>`. `NetworkManager` verbindet genau eine gespeicherte SSID, protokolliert Statuswechsel und versucht Verbindungen mit begrenzter Wiederholrate erneut. Nach einer Verbindung werden SSID, BSSID, Kanal, RSSI, IP, Gateway und DNS ausgegeben. Es gibt keine Scan-, Multi-SSID- oder Roaming-Sonderlogik.

Ohne gespeicherte Zugangsdaten startet `FERM-01-Setup` als SoftAP mit dem bestehenden HTTP-Setup-Portal. Gateway-Erkennung bleibt DNS-SD über Multicast-UDP. Das bestehende WebSocket-Protokoll (`REGISTER_SENSOR`, ACKs, Runtime State, Measurements und Bubble Activity) bleibt unverändert und verwendet `ArduinoHttpClient` mit dem ESP32-`WiFiClient`.

## Persistenz

`FlashStorage` verwendet `Preferences` und damit den nativen ESP32-NVS-Namespace `fermsensor`. Kurze fachliche Schlüssel bleiben lesbar; die drei zu langen Schlüssel werden explizit und kollisionsfrei auf dokumentierte NVS-Schlüssel abgebildet. Factory Reset löscht ausschließlich diesen Namespace. Es gibt keine Migration von früheren Geräten.

Die beim ersten Boot erzeugte UUID v4 wird im `device_config`-Record gespeichert und danach wieder geladen; die MAC-Adresse ist nicht die Geräte-ID. Der Measurement-Sequence-Allocator reserviert weiterhin einen kompletten Block dauerhaft **vor** dessen erster Verwendung. Dadurch überspringt ein Neustart den zuletzt reservierten Bereich und gibt keine möglicherweise bereits verwendete Sequence erneut aus.

Weitere Details: [Konfiguration](docs/configuration.md), [NVS-Speicher](docs/storage.md), [Netzwerkdiagnose](docs/network-disconnect-diagnosis.md).

## Hardwaretests

Ohne echte Hardware lassen sich Build und Hosttests prüfen. Auf einem Nano ESP32 müssen zusätzlich USB-Upload/CDC, SoftAP und Station-Verbindung, DNS-SD/WebSocket gegen das reale Gateway, RSSI/BSSID/Kanal, NVS über Power-Cycles, beide DS18B20, LWLP5000-I2C, Button sowie alle drei externen LEDs geprüft werden.
