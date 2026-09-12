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
    _sensors.setWaitForConversion(false);

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

bool TemperatureSensor::update()
{
    const unsigned long now =
        millis();

    if (!_conversionInProgress) {
        if (
            now - _lastMeasurementMs <
            TEMPERATURE_INTERVAL_MS
        ) {
            return false;
        }

        _lastMeasurementMs = now;
        _conversionStartedMs = now;
        _conversionTimeMs = getConversionTimeMs();
        _conversionInProgress = true;

        _sensors.requestTemperatures();

        return false;
    }

    if (
        now - _conversionStartedMs <
        _conversionTimeMs
    ) {
        return false;
    }

    _conversionInProgress = false;


    const int sensorCount =
        _sensors.getDeviceCount();


    bool ambientMeasurementReceived =
        false;

    bool beerMeasurementReceived =
        false;

    _measurementAttempted = true;


    for (int i = 0; i < sensorCount; ++i) {
        DeviceAddress address;


        if (!_sensors.getAddress(address, i)) {
            continue;
        }


        const float temperature =
            _sensors.getTempC(address);


        if (isAmbientSensor(address)) {
            if (temperature == DEVICE_DISCONNECTED_C) {
                Serial.println(
                    "Ambient temperature sensor returned an invalid value."
                );

                continue;
            }

            _ambientTemperature =
                temperature;

            ambientMeasurementReceived =
                true;           
        }
        else if (isBeerSensor(address)) {
            if (temperature == DEVICE_DISCONNECTED_C) {
                Serial.println(
                    "Beer temperature sensor returned an invalid value."
                );

                continue;
            }

            _beerTemperature =
                temperature;

            beerMeasurementReceived =
                true;           
        }
        else {
            Serial.print(
                "Unknown sensor "
            );           
        }
    }


    _lastMeasurementValid =
        ambientMeasurementReceived &&
        beerMeasurementReceived;

    return _lastMeasurementValid;
}


unsigned long TemperatureSensor::getConversionTimeMs()
{
    uint8_t highestResolution = 9;

    const int sensorCount =
        _sensors.getDeviceCount();

    for (int index = 0; index < sensorCount; ++index) {
        DeviceAddress address;

        if (!_sensors.getAddress(address, index)) {
            continue;
        }

        const uint8_t resolution =
            _sensors.getResolution(address);

        if (resolution > highestResolution) {
            highestResolution = resolution;
        }
    }

    switch (highestResolution) {
        case 9:
            return 94;

        case 10:
            return 188;

        case 11:
            return 375;

        default:
            return 750;
    }
}

bool TemperatureSensor::areAllSensorsConnected()
{
    if (
        _measurementAttempted &&
        !_lastMeasurementValid
    ) {
        return false;
    }

    // Solange noch kein Ambient-Sensor konfiguriert
    // wurde, ist die Sensorkonfiguration nicht vollständig.
    if (!_ambientSensorId.isValid()) {
        return false;
    }

    if (!_beerSensorId.isValid()) {
        return false;
    }

    if (!isSensorConnected(_ambientSensorId)) {
        return false;
    }

    if (!isSensorConnected(_beerSensorId)) {
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
    if (_conversionInProgress) {
        return;
    }

    _sensors.begin();
}


bool TemperatureSensor::isConversionInProgress() const
{
    return _conversionInProgress;
}

float TemperatureSensor::getBeerTemperature() const
{
    return _beerTemperature;
}


float TemperatureSensor::getAmbientTemperature() const
{
    return _ambientTemperature;
}
