# Sensor-Konfiguration

Dieses Dokument beschreibt die zur Laufzeit änderbaren technischen Parameter des
`FermentationSensor`, ihre Wirkung und die gültigen Wertebereiche.

Die Konfiguration betrifft ausschließlich die **technische Messung und Diagnose**.
Sie enthält keine fachliche Bewertung der Gärung. Aussagen wie „Gärung aktiv“,
„Gärung beendet“, Plato-Auswertung oder Cold-Crash-Empfehlungen gehören nicht in
den Sensor.

## Zugriff

Nach erfolgreicher WLAN-Verbindung stellt der Sensor auf Port 80 eine kleine lokale
Konfigurationsoberfläche bereit:

```text
http://<sensor-ip>/
```

Je nach lokaler Namensauflösung kann zusätzlich der technische Hostname erreichbar
sein, zum Beispiel:

```text
http://FERM-1FF77E.local/
```

Die HTTP-Endpunkte sind:

| Methode | Endpoint | Zweck |
| --- | --- | --- |
| `GET` | `/` | Lokale HTML-Konfigurationsseite |
| `GET` | `/api/config` | Aktuelle technische Sensor-Konfiguration |
| `POST` | `/api/config` | Vollständige technische Konfiguration speichern |
| `GET` | `/api/device` | UUID und Anzeigename lesen |
| `PUT` | `/api/device/name` | Anzeigenamen ändern |

Änderungen an `/api/config` sind nur möglich, wenn sich die Messsession in
`IDLE` befindet. In `RUNNING` oder `PAUSED` antwortet der Sensor mit HTTP `409`.
Dadurch werden Kalibrierung, Bubble-Erkennung und Aggregationsfenster nicht mitten
in einer laufenden Messung umkonfiguriert.

## Persistenz

Die technische Konfiguration wird als **ein gemeinsamer Record** gespeichert:

```text
sensor_config_v1
```

Der Record enthält:

- Magic `SCFG`,
- Config-Version `1`,
- Record-Größe,
- sämtliche Werte aus `SensorConfig`.

Ein Write wird anschließend vollständig aus dem Flash zurückgelesen und bytegleich
verifiziert. Erst danach wird die neue Konfiguration zur Laufzeit aktiviert.
Identische Konfigurationen erzeugen keinen erneuten Flash-Write.

Fehlt der Record oder ist er ungültig, verwendet der Sensor die sicheren Defaults.
Die maßgebliche Implementierung befindet sich in:

- `include/config/SensorConfig.h`
- `src/config/SensorConfig.cpp`
- `src/storage/SensorConfigStore.cpp`

## Übersicht der konfigurierbaren Werte

| Bereich | API-Feld | Default | Gültiger Bereich | Bedeutung |
| --- | --- | ---: | ---: | --- |
| Diagnose | `events` | `true` | Boolean | Ereignisdiagnose für Kalibrierung, Bubble-Events und Aktivitätsfenster |
| Diagnose | `rawPressure` | `false` | Boolean | Hochfrequente serielle Rohdruckausgabe |
| Druck | `sampleIntervalMs` | `100 ms` | `1 … 60000 ms` | Abstand zwischen Druckmessungen |
| Druck | `calibrationMs` | `300000 ms` | `1 … 3600000 ms` | Dauer der Druck-Kalibrierung |
| Druck | `minTriggerDeltaPa` | `0.50 Pa` | `> 0 … 10000 Pa` | Untere Mindestschwelle für einen Bubble-Trigger |
| Druck | `noiseFactor` | `5.0` | `> 0 … 100` | Verstärkung des während der Kalibrierung gemessenen Rauschens |
| Druck | `releaseFactor` | `0.40` | `0 < Wert < 1` | Faktor für die Rückkehrschwelle eines Bubble-Events |
| Druck | `minDurationMs` | `100 ms` | `1 … 60000 ms` | Mindestdauer eines gültigen Bubble-Events |
| Druck | `maxDurationMs` | `3000 ms` | `>= minDurationMs`, max. `300000 ms` | Maximale Dauer eines gültigen Bubble-Events |
| Druck | `refractoryMs` | `500 ms` | `0 … 60000 ms` | Sperrzeit nach einem Event |
| Druck | `baselineTrackingAlpha` | `0.001` | `0 < Wert < 1` | Geschwindigkeit der langsamen Baseline-Nachführung für die Bubble-Erkennung |
| Aggregation | `windowMs` | `60000 ms` | `1 … 3600000 ms` | Länge eines technischen Aktivitätsfensters |
| Temperatur | `sendDeltaC` | `1.0 °C` | `> 0 … 50 °C` | Temperaturänderung, ab der erneut übertragen wird |

Alle Fließkommazahlen müssen endlich sein. `NaN` und `Inf` werden abgelehnt.

## Diagnose

### `events`

Default: `true`

Aktiviert die sinnvollen ereignisbasierten seriellen Diagnoseausgaben. Dazu gehören
unter anderem:

```text
PRESSURE_CALIBRATED,...
BUBBLE,...
BUBBLE_WINDOW,...
```

Diese Ausgaben entstehen nur bei entsprechenden Ereignissen und sind deshalb auch
im normalen Testbetrieb gut nutzbar.

### `rawPressure`

Default: `false`

Aktiviert die hochfrequente Rohdruckausgabe:

```text
PRESSURE,<millis>,<pressurePa>
```

Bei `sampleIntervalMs = 100` entstehen ungefähr zehn Zeilen pro Sekunde. Diese
Ausgabe ist für Diagnose und Kalibrierung gedacht und sollte im normalen Betrieb
ausgeschaltet bleiben.

**Wichtig:** Das Abschalten von `rawPressure` beendet nicht die interne
Druckmessung. Sampling, Bubble-Erkennung und Druckaggregation laufen unverändert
weiter.

## Druckmessung und Bubble-Erkennung

### `sampleIntervalMs`

Bestimmt, wie häufig der Differenzdrucksensor ausgelesen wird.

Default:

```text
100 ms = 10 Messungen pro Sekunde
```

Ein kleinerer Wert erhöht die zeitliche Auflösung, erzeugt aber mehr Sensorzugriffe
und bei aktivierter Rohdiagnose entsprechend mehr serielle Ausgaben. Ein deutlich
größerer Wert kann kurze Druckereignisse schlechter erfassen.

Für das aktuelle System ist `100 ms` der erprobte Ausgangswert.

### `calibrationMs`

Bestimmt die Dauer der Kalibrierungsphase nach dem Start einer Messsession.

Default:

```text
300000 ms = 5 Minuten
```

Während dieser Zeit bestimmt der Sensor aus Druckblöcken:

- die Druck-Baseline,
- das typische Rauschen,
- daraus die effektive Trigger- und Release-Schwelle.

Erst nach abgeschlossener Kalibrierung beginnt die normale Bubble-Erkennung und
das erste Aktivitätsfenster.

Eine längere Kalibrierung liefert mehr Daten für die Ausgangsbewertung, verzögert
aber den Beginn der regulären Aktivitätsmessung. Eine zu kurze Kalibrierung kann
bei unruhigem System zu weniger stabilen Schwellen führen.

### `minTriggerDeltaPa`

Absolute Mindestabweichung oberhalb der aktuellen Detektions-Baseline, die für
einen Bubble-Start erforderlich ist.

Die tatsächlich verwendete Trigger-Schwelle wird nach der Kalibrierung berechnet:

```text
triggerDeltaPa = max(minTriggerDeltaPa, noisePa * noiseFactor)
```

Damit kann die Erkennung niemals empfindlicher werden als durch
`minTriggerDeltaPa` vorgegeben, wird bei einem stärker rauschenden Signal aber
automatisch unempfindlicher.

Höherer Wert:

- weniger empfindlich,
- weniger Fehltrigger durch kleine Druckänderungen,
- schwache echte Ereignisse können übersehen werden.

Niedrigerer Wert:

- empfindlicher,
- kleine Ereignisse werden leichter erkannt,
- höhere Gefahr von Fehltriggern.

### `noiseFactor`

Multiplikator für das während der Kalibrierung bestimmte Druckrauschen.

Beispiel:

```text
noisePa = 0.12 Pa
noiseFactor = 5.0

noisePa * noiseFactor = 0.60 Pa
```

Bei `minTriggerDeltaPa = 0.50 Pa` ergibt sich dadurch eine effektive
Trigger-Schwelle von `0.60 Pa`.

Ein höherer Faktor macht die Erkennung robuster gegen Rauschen, aber weniger
empfindlich. Ein niedrigerer Faktor macht sie empfindlicher.

`minTriggerDeltaPa` und `noiseFactor` sollten deshalb immer gemeinsam betrachtet
werden.

### `releaseFactor`

Bestimmt, wie weit der Druck nach einem Trigger wieder zurückgehen muss, bevor das
Ereignis als beendet betrachtet wird.

Berechnung:

```text
releaseDeltaPa = triggerDeltaPa * releaseFactor
```

Bei einem Trigger von `1.0 Pa` und dem Default `0.40` liegt die Release-Schwelle
bei `0.40 Pa`.

Ein kleinerer Wert verlangt eine stärkere Rückkehr Richtung Baseline und kann ein
Ereignis länger offen halten. Ein größerer Wert beendet es früher.

Der Wert muss strikt zwischen `0` und `1` liegen.

### `minDurationMs`

Ein Druckereignis muss mindestens so lange aktiv gewesen sein, bevor es als
gültiger Bubble gezählt wird.

Default:

```text
100 ms
```

Sehr kurze Peaks unterhalb dieses Werts werden verworfen. Der Parameter dient
hauptsächlich dazu, elektrische oder mechanische Spitzen nicht als Bubble zu
zählen.

### `maxDurationMs`

Maximale Dauer eines gültigen Bubble-Ereignisses.

Default:

```text
3000 ms = 3 Sekunden
```

Bleibt der Druck länger oberhalb der relevanten Schwelle, wird dieses Ereignis
nicht als Bubble gezählt. Der Detektor wechselt anschließend in die Refractory-
Phase.

Damit werden lang anhaltende Druckänderungen nicht automatisch als einzelne
extrem lange Blase interpretiert.

`maxDurationMs` darf nicht kleiner als `minDurationMs` sein.

### `refractoryMs`

Sperrzeit direkt nach einem abgeschlossenen oder verworfenen Event.

Default:

```text
500 ms
```

Während dieser Zeit beginnt kein neues Bubble-Event. Das reduziert
Doppelzählungen durch Nachschwingen desselben Druckimpulses.

Ein zu hoher Wert kann schnell aufeinanderfolgende echte Ereignisse unterdrücken.
Ein zu niedriger Wert kann Nachschwingen eher mehrfach zählen.

### `baselineTrackingAlpha`

Steuert die langsame adaptive Nachführung der **Detektions-Baseline** im normalen
Monitoring.

Vereinfacht:

```text
baseline += alpha * (pressure - baseline)
```

Die Anpassung erfolgt nur, solange kein Bubble-Event aktiv ist und die Abweichung
unterhalb der Trigger-Schwelle liegt.

Default:

```text
0.001
```

Kleinerer Wert:

- Baseline folgt langsamer,
- langfristige Stabilität höher,
- langsame reale Drift wird später ausgeglichen.

Größerer Wert:

- Baseline folgt schneller,
- langsame technische Drift wird schneller kompensiert,
- zu große Werte können langsame Druckänderungen stärker „wegregeln“.

**Wichtig:** Diese adaptive Detektions-Baseline ist nicht identisch mit der für
die Aktivitätsfenster verwendeten festen Kalibrierungsreferenz. Der gemittelte
Differenzdruck eines `BUBBLE_ACTIVITY`-Fensters bleibt relativ zur bei der
Kalibrierung bestimmten Referenz und wird nicht durch die adaptive
Detektions-Baseline nivelliert.

## Aggregation

### `windowMs`

Bestimmt die Dauer eines technischen `BUBBLE_ACTIVITY`-Fensters.

Default:

```text
60000 ms = 60 Sekunden
```

Innerhalb dieses Fensters werden unter anderem zusammengefasst:

- erkannte Bubble-Events,
- der mittlere Differenzdruck relativ zur festen Kalibrierungsreferenz.

Die Fensterzeit zählt nur aktive `RUNNING`-Zeit. Eine Pause verlängert das
Fenster in realer Wandzeit entsprechend; die Pause selbst wird nicht als aktive
Messzeit eingerechnet.

Der Default von 60 Sekunden ist besonders übersichtlich, weil er direkt einem
Minutenfenster entspricht. Bei einer Änderung muss die auswertende UI immer die
mitgelieferte tatsächliche Fensterdauer berücksichtigen.

## Temperatur

### `sendDeltaC`

Bestimmt, wie stark sich mindestens eine Temperatur ändern muss, bevor erneut eine
Temperaturmessung in die Measurement-Outbox eingestellt wird.

Default:

```text
1.0 °C
```

Verglichen wird mit der zuletzt **erfolgreich in die Outbox übernommenen**
Messung, nicht mit dem unmittelbar vorherigen lokalen Messwert.

Beispiel:

```text
zuletzt übertragen: 20.0 °C
lokal gemessen:      20.4 °C -> keine neue Übertragung
lokal gemessen:      20.8 °C -> keine neue Übertragung
lokal gemessen:      21.0 °C -> neue Übertragung
```

Die Schwelle gilt für Bier- oder Umgebungstemperatur. Erreicht eine der beiden
Temperaturen die Schwelle, wird das gültige Temperaturpaar übertragen.

Die erste gültige Messung und ausdrücklich angeforderte aktuelle Snapshots können
unabhängig von dieser Schwelle übertragen werden.

## Beispiel für `GET /api/config`

```json
{
  "version": 1,
  "diagnostics": {
    "events": true,
    "rawPressure": false
  },
  "pressure": {
    "sampleIntervalMs": 100,
    "calibrationMs": 300000,
    "minTriggerDeltaPa": 0.5,
    "noiseFactor": 5.0,
    "releaseFactor": 0.4,
    "minDurationMs": 100,
    "maxDurationMs": 3000,
    "refractoryMs": 500,
    "baselineTrackingAlpha": 0.001
  },
  "aggregation": {
    "windowMs": 60000
  },
  "temperature": {
    "sendDeltaC": 1.0
  }
}
```

## Beispiel für `POST /api/config`

Der Endpoint erwartet immer den vollständigen Parametersatz:

```json
{
  "version": 1,
  "diagnostics": {
    "events": true,
    "rawPressure": false
  },
  "pressure": {
    "sampleIntervalMs": 100,
    "calibrationMs": 300000,
    "minTriggerDeltaPa": 0.5,
    "noiseFactor": 5.0,
    "releaseFactor": 0.4,
    "minDurationMs": 100,
    "maxDurationMs": 3000,
    "refractoryMs": 500,
    "baselineTrackingAlpha": 0.001
  },
  "aggregation": {
    "windowMs": 60000
  },
  "temperature": {
    "sendDeltaC": 1.0
  }
}
```

Erfolgreich geändert:

```json
{
  "success": true,
  "changed": true
}
```

Werte waren bereits identisch:

```json
{
  "success": true,
  "changed": false
}
```

Typische Fehler:

- `400 Bad Request`: JSON unvollständig, falscher Typ oder ungültiger Wert,
- `409 Conflict`: Messsession ist nicht `IDLE`,
- `413 Payload Too Large`: Header oder Body überschreiten die Servergrenze,
- `500 Internal Server Error`: persistentes Speichern oder Verifizieren ist fehlgeschlagen.

Die beiden Diagnosewerte müssen echte JSON-Booleans (`true`/`false`) sein. `0`,
`1` oder Strings werden nicht als Boolean akzeptiert.

## Gerätename

Der editierbare Gerätename gehört bewusst **nicht** zu `sensor_config_v1`. Er ist
Teil der persistenten Geräteidentität im bestehenden Record:

```text
device_config
```

Lesen:

```http
GET /api/device
```

Beispiel:

```json
{
  "deviceId": "307528d6-76af-4616-a0c1-568e471ff77e",
  "deviceName": "Gärtank Keller"
}
```

Ändern:

```http
PUT /api/device/name
Content-Type: application/json
```

```json
{
  "name": "Gärtank Keller"
}
```

Für den Namen gelten aktuell folgende Regeln:

- führende und nachfolgende Leerzeichen werden entfernt,
- mindestens ein Zeichen,
- maximal 63 Bytes/Zeichen im aktuellen `Arduino String`-Handling,
- erlaubt sind Buchstaben, Ziffern, Leerzeichen, `-`, `_`, `.`, `(` und `)`,
- Nicht-ASCII-Bytes ab `0x80` werden akzeptiert,
- die UUID kann über diesen Endpoint niemals geändert werden.

Wird der Name tatsächlich geändert, wird der persistierte Wert zuerst gespeichert
und verifiziert. Anschließend veranlasst der Sensor einen kontrollierten
Gateway-Reconnect, sodass die nächste `REGISTER_SENSOR`-Nachricht denselben
`deviceId`, aber den neuen `deviceName` enthält.

Der Anzeigename ist **nicht** der technische Netzwerkname. Eine Namensänderung
ändert weder die UUID noch den mDNS-/Discovery-Endpunkt.

## Nicht konfigurierbare Werte

Folgende Werte sind bewusst nicht Bestandteil der lokalen Runtime-Konfiguration
und werden von `POST /api/config` nicht akzeptiert:

- Geräte-UUID,
- WLAN-SSID und WLAN-Passwort,
- Measurement-Sequence-State,
- Sequence-Blockgröße und Initialbereich,
- Flash-Adressen und Storage-Layout,
- Protokollversion,
- Hardware-Pins,
- Outbox-Kapazität,
- ACK-Timeout.

Diese Trennung schützt Identität, Persistenz und Transportprotokoll vor
versehentlichen Änderungen über die Bedienoberfläche.

## Empfehlung für die Parametrierung

Die Defaults sind der aktuelle Ausgangspunkt für den realen Sensorbetrieb. Bei der
Optimierung sollten Werte einzeln und mit nachvollziehbaren Messreihen verändert
werden.

Besonders sinnvoll ist folgende Reihenfolge:

1. `rawPressure` nur temporär aktivieren und eine reale Druckkurve beobachten.
2. Rauschpegel und echte Bubble-Amplituden vergleichen.
3. Falls nötig `minTriggerDeltaPa` und `noiseFactor` anpassen.
4. Danach erst Dauer- und Refractory-Werte verändern.
5. `baselineTrackingAlpha` nur ändern, wenn eine langsame technische Drift der
   Detektions-Baseline tatsächlich beobachtet wird.
6. Nach einer Änderung eine vollständige neue Kalibrierung und reale Messsession
   prüfen.

Die technische Bubble-Erkennung ist dabei nur ein Messsignal. Aus einzelnen
Parametern, Bubble-Zahlen oder Differenzdruckwerten darf der Sensor selbst keinen
fachlichen Gärstatus ableiten.
