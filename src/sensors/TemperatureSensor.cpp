#include "sensors/TemperatureSensor.h"

#include "config/Config.h"

TemperatureSensor::TemperatureSensor(
    uint8_t pin,
    TemperatureSensorStore& sensorStore
)
    : _pin(pin),
      _oneWire(pin),
      _sensors(&_oneWire),
      _sensorStore(sensorStore)
{
}

void TemperatureSensor::begin()
{
    _sensors.begin();

    const int sensorCount =
        _sensors.getDeviceCount();

    Serial.print("DS18B20 sensors found: ");
    Serial.println(sensorCount);

    if (_sensorStore.hasAmbientSensor()) {
        _ambientSensorId =
            _sensorStore.loadAmbientSensor();

        Serial.print("Stored ambient sensor: ");

        DeviceAddress address;

        for (uint8_t i = 0; i < 8; ++i) {
            address[i] = _ambientSensorId.bytes[i];
        }

        printSensorAddress(address);
        Serial.println();
    }

    if (_sensorStore.hasBeerSensor()) {
        _beerSensorId =
            _sensorStore.loadBeerSensor();

        Serial.print("Stored beer sensor: ");

        DeviceAddress address;

        for (uint8_t i = 0; i < 8; ++i) {
            address[i] = _beerSensorId.bytes[i];
        }

        printSensorAddress(address);
        Serial.println();
    }

    for (int i = 0; i < sensorCount; ++i) {
        DeviceAddress address;

        if (!_sensors.getAddress(address, i)) {
            continue;
        }

        Serial.print("Sensor ");
        Serial.print(i);
        Serial.print(" address: ");

        printSensorAddress(address);
        Serial.println();
    }

    // Wenn noch kein Ambient-Sensor bekannt ist
    // und genau ein Sensor angeschlossen ist,
    // wird dieser als Ambient-Sensor gelernt.
    if (
        !_ambientSensorId.isValid() &&
        sensorCount == 1
    ) {
        DeviceAddress address;

        if (_sensors.getAddress(address, 0)) {
            const TemperatureSensorId sensorId =
                toSensorId(address);

            if (_sensorStore.saveAmbientSensor(sensorId)) {
                _ambientSensorId = sensorId;

                Serial.print(
                    "Ambient sensor learned: "
                );

                printSensorAddress(address);
                Serial.println();
            }
        }
    }

    // Wenn Ambient bekannt ist, suchen wir nach
    // weiteren Sensoren.
    //
    // Genau ein weiterer Sensor:
    // -> als Beer-Sensor lernen.
    //
    // Mehrere unbekannte Sensoren:
    // -> keine automatische Zuordnung.
    if (_ambientSensorId.isValid()) {
        int unknownCount = 0;

        TemperatureSensorId unknownSensorId;
        DeviceAddress unknownAddress;

        for (int i = 0; i < sensorCount; ++i) {
            DeviceAddress address;

            if (!_sensors.getAddress(address, i)) {
                continue;
            }

            const TemperatureSensorId sensorId =
                toSensorId(address);

            if (sensorId.equals(_ambientSensorId)) {
                continue;
            }

            ++unknownCount;

            unknownSensorId = sensorId;

            for (uint8_t j = 0; j < 8; ++j) {
                unknownAddress[j] = address[j];
            }
        }

        if (unknownCount == 1) {
            if (
                !_beerSensorId.isValid() ||
                !unknownSensorId.equals(_beerSensorId)
            ) {
                if (
                    _sensorStore.saveBeerSensor(
                        unknownSensorId
                    )
                ) {
                    _beerSensorId =
                        unknownSensorId;

                    Serial.print(
                        "Beer sensor learned: "
                    );

                    printSensorAddress(
                        unknownAddress
                    );

                    Serial.println();
                }
            }
        }
        else if (unknownCount > 1) {
            Serial.println(
                "Multiple unknown temperature sensors found."
            );
        }
    }

    // Aktuellen Verbindungsstatus ausgeben.
    if (_ambientSensorId.isValid()) {
        Serial.print("Ambient sensor: ");

        if (isSensorConnected(_ambientSensorId)) {
            Serial.println("connected");
        }
        else {
            Serial.println("disconnected");
        }
    }

    if (_beerSensorId.isValid()) {
        Serial.print("Beer sensor: ");

        if (isSensorConnected(_beerSensorId)) {
            Serial.println("connected");
        }
        else {
            Serial.println("disconnected");
        }
    }
}

void TemperatureSensor::update()
{
    const unsigned long now = millis();

    if (
        now - _lastMeasurementMs <
        TEMPERATURE_INTERVAL_MS
    ) {
        return;
    }

    _lastMeasurementMs = now;

    _sensors.requestTemperatures();

    const int sensorCount =
        _sensors.getDeviceCount();

    for (int i = 0; i < sensorCount; ++i) {
        DeviceAddress address;

        if (!_sensors.getAddress(address, i)) {
            continue;
        }

        const float temperature =
            _sensors.getTempC(address);

        if (isAmbientSensor(address)) {
            Serial.print(
                "Ambient temperature: "
            );

            Serial.print(temperature);
            Serial.println(" C");
        }
        else if (isBeerSensor(address)) {
            Serial.print(
                "Beer temperature: "
            );

            Serial.print(temperature);
            Serial.println(" C");
        }
        else {
            Serial.print("Unknown sensor ");

            printSensorAddress(address);

            Serial.print(": ");
            Serial.print(temperature);
            Serial.println(" C");
        }
    }
}

bool TemperatureSensor::areAllSensorsConnected()
{
    // Solange noch kein Ambient-Sensor konfiguriert
    // wurde, ist die Sensorkonfiguration nicht vollständig.
    if (!_ambientSensorId.isValid()) {
        return false;
    }

    if (!isSensorConnected(_ambientSensorId)) {
        return false;
    }

    // Sobald ein Beer-Sensor gelernt wurde,
    // erwarten wir auch diesen Sensor.
    if (
        _beerSensorId.isValid() &&
        !isSensorConnected(_beerSensorId)
    ) {
        return false;
    }

    return true;
}

void TemperatureSensor::printSensorAddress(
    const DeviceAddress& address
)
{
    for (uint8_t i = 0; i < 8; ++i) {
        if (address[i] < 16) {
            Serial.print('0');
        }

        Serial.print(address[i], HEX);

        if (i < 7) {
            Serial.print(':');
        }
    }
}

TemperatureSensorId
TemperatureSensor::toSensorId(
    const DeviceAddress& address
)
{
    TemperatureSensorId sensorId;

    for (uint8_t i = 0; i < 8; ++i) {
        sensorId.bytes[i] = address[i];
    }

    return sensorId;
}

bool TemperatureSensor::isAmbientSensor(
    const DeviceAddress& address
)
{
    if (!_ambientSensorId.isValid()) {
        return false;
    }

    return _ambientSensorId.equals(
        toSensorId(address)
    );
}

bool TemperatureSensor::isBeerSensor(
    const DeviceAddress& address
)
{
    if (!_beerSensorId.isValid()) {
        return false;
    }

    return _beerSensorId.equals(
        toSensorId(address)
    );
}

bool TemperatureSensor::isSensorConnected(
    const TemperatureSensorId& sensorId
)
{
    if (!sensorId.isValid()) {
        return false;
    }

    const int sensorCount =
        _sensors.getDeviceCount();

    for (int i = 0; i < sensorCount; ++i) {
        DeviceAddress address;

        if (!_sensors.getAddress(address, i)) {
            continue;
        }

        const TemperatureSensorId currentSensorId =
            toSensorId(address);

        if (currentSensorId.equals(sensorId)) {
            return true;
        }
    }

    return false;
}

void TemperatureSensor::refresh()
{
    _sensors.begin();
}