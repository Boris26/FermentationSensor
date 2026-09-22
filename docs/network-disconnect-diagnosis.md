# Diagnose der regelmäßigen WebSocket-Abbrüche

## Ergebnis der statischen Analyse

Die Firmware enthält **keinen 1,4- oder 1,5-Sekunden-Timeout**, der eine bestehende
WebSocket-Verbindung beendet. Die ähnlich großen Konstanten gehören zum Blinken der
Backend-Fehler-LED (`1500 ms`) und zum inaktiven HTTP-Konfigurationsclient (`1500 ms`).
Beide rufen den `ServerClient` nicht auf. Der WebSocket-Reconnect beginnt mit 1000 ms,
dieser Wert wirkt aber erst, nachdem die Verbindung bereits verloren wurde.

Eine bestehende Verbindung wird lokal nur in folgenden Fällen geschlossen:

* `WebSocketClient::connected()` meldet den darunterliegenden Transport als getrennt;
* der Registrierungs-ACK fehlt 7500 ms lang;
* ein WebSocket-Frame kann nicht begonnen, vollständig geschrieben oder beendet werden;
* ein empfangener Frame überschreitet den 256-Byte-Puffer;
* WLAN geht verloren, die Konfiguration fordert einen Reconnect an oder der Client wird
  absichtlich gestoppt.

Damit passt der beobachtete Abstand von etwa 1,46 Sekunden zu keinem lokalen
Registrierungs-, Messungs-ACK-, Heartbeat- oder Reconnect-Timeout. Insbesondere ist der
Messungs-ACK-Timeout mit 5000 ms nur ein Retry der gepufferten Messung und trennt den
Socket nicht. `HEARTBEAT_INTERVAL_MS` war und ist in dieser Firmware nicht an einen
Sender gekoppelt; die Firmware erzeugt also selbst keinen periodischen Ping.

Aus dem Repository allein lässt sich nicht unterscheiden, ob das Gateway den TCP-/WS-
Transport schließt oder ob der ESP32-WiFi-Transport ihn verliert. Der bisherige Code warf die
entscheidende Information weg: Er prüfte nur `connected() == false` und schrieb danach
die allgemeine Meldung `disconnected`. Die neuen Lifecycle-Logs erfassen deshalb den
lokalen Auslöser, WLAN-Status, RSSI, Socket-Zustand und das Alter der letzten RX-/TX-
Aktivität. Ein Disconnect mit `wifiStatus=WL_CONNECTED` trennt Socket und WLAN klar;
`reason=underlying TCP/WebSocket not connected` bedeutet, dass nicht die Firmware-
State-Machine `stop()` aufgerufen hat, sondern der Transport beim Polling bereits zu war.

## Blockierungsanalyse

* Der DS18B20 ist bereits asynchron konfiguriert (`setWaitForConversion(false)`). Eine
  Konvertierung wird gestartet und erst nach der auflösungsabhängigen Frist in einem
  späteren Loop gelesen. Die typische maximale 12-Bit-Konvertierungszeit von 750 ms
  blockiert den Loop daher nicht.
* Der Drucktreiber blockiert bei einem fälligen Sample absichtlich 30 ms während der
  Sensor-Konvertierung. Hinzu kommt die Dauer von `Wire.endTransmission()` und
  `Wire.requestFrom()`, deren Fehler-Worst-Case von der Wire-Implementierung abhängt.
  Es existieren keine Retry- oder Ready-Warteschleifen im Treiber.
* Der WebSocket-Connect/Handshake, `WiFi.begin()` und einzelne Socket-Schreibvorgänge
  sind Bibliotheksaufrufe und können synchron warten. Im stabil verbundenen Zustand
  pollt `parseMessage()` einmal je Loop. Discovery, Setup-Portal und Konfigurationsserver
  arbeiten mit begrenzten Lese-Budgets.
* Serielle Diagnoseausgaben können bei vollem USB-/UART-Ausgabepuffer blockieren. Die
  neuen Detailausgaben entstehen daher nur bei Zustandsänderungen, Frames, Fehlern und
  Loops über 100 ms, nicht in jedem normalen Loop.

Die Loop-Messung weist bei jedem Lauf über 100 ms die Zeit den einzelnen
Anwendungsabschnitten zu und unterscheidet zusätzlich die Schwellen 500 und 1000 ms.
Damit wird insbesondere sichtbar, ob `ServerClient/Gateway.update`,
`TemperatureSensor.update` oder `Session/PressureSensor.update` die Lücke verursacht.

## Reconnect und erneutes `RUNNING`

Nach einem erkannten Transportverlust setzt `disconnect()` Registrierung und
Registrierungsereignis zurück und stoppt WebSocket sowie TCP. `update()` versucht den
Reconnect nach dem Backoff erneut. Jeder Connect sendet zwingend eine neue
`REGISTER_SENSOR`-Nachricht. Nach `REGISTER_SENSOR_ACK` setzt der Client einmalig das
Ereignis `registrationEstablished`; der Session-Coordinator überträgt daraufhin den
aktuellen lokalen Zustand absichtlich mit `force=true`. Deshalb erscheint `RUNNING`
nach jedem Reconnect erneut. Es startet weder die lokale Session noch die
Druckkalibrierung neu, sondern ist ein erwarteter Initial-Sync und damit ein Symptom der
Reconnects.

Die untersuchten Timer werden bei erfolgreichem Connect beziehungsweise Versand neu
gesetzt. Ein alter Registrierungs-Timeout kann nicht über den Reconnect hinweg feuern,
weil `sendRegistration()` den Zeitstempel neu setzt. Der Reconnect-Pfad wird nur bei
`_connected == false` betreten; er reconnectet nicht parallel zu einer als aktiv
markierten Verbindung.

## Prüfung am Gerät

1. Seriellen Monitor mit 115200 Baud öffnen und mindestens mehrere Minuten laufen
   lassen.
2. Auf `WIFI_STATUS_CHANGED`, `WS_CONNECTED`, `WS_TX`, `WS_RX`, `SLOW_LOOP` und
   `WS_DISCONNECTED` achten.
3. Bei `WS_DISCONNECTED` `reason`, `wifiStatus`, `socketConnected`, `lastRxAge` und
   `lastTxAge` gemeinsam auswerten. Bleibt WLAN verbunden und ist kein `SLOW_LOOP` in
   derselben Zeitspanne vorhanden, muss der Close auf Gateway-/TCP-Ebene mit den
   Gateway-Logs beziehungsweise einem Paketmitschnitt korreliert werden.
4. Für einen echten Recovery-Test das Gateway kurz stoppen beziehungsweise WLAN
   gezielt abschalten. Danach müssen ein Statuswechsel, Backoff-Versuche, ein neuer
   `WS_CONNECTED` und genau ein Runtime-State-Sync nach dem Registrierungs-ACK folgen.

Ohne Laufzeitlog vom betroffenen Arduino und korreliertes Gateway-Log wäre eine
Behauptung über den konkreten externen Close-Grund spekulativ. Deshalb wurde bewusst
weder ein Timeout erhöht noch der Disconnect verborgen oder ein unbestätigter
Heartbeat-Fix eingebaut.
