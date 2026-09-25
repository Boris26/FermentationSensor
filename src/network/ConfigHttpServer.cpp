#include "network/ConfigHttpServer.h"
#include "ConfigPage.h"
#include "network/ServerClient.h"
#include "sensors/PressureSensor.h"
#include "sensors/TemperatureSensor.h"

#include <cmath>
#include <cstdlib>

namespace {
constexpr uint16_t PORT = 80;
constexpr size_t MAX_HEADERS = 1024, MAX_BODY = 2048, BYTE_BUDGET = 256;

bool valueStart(const String& json, const char* key, int& start)
{
    const String needle = String("\"") + key + "\"";
    int p = json.indexOf(needle);
    if (p < 0 || json.indexOf(needle, p + needle.length()) >= 0) return false;
    p = json.indexOf(':', p + needle.length());
    if (p < 0) return false;
    start = p + 1;
    while (start < static_cast<int>(json.length()) && isspace(json[start])) ++start;
    return true;
}
bool readBool(const String& json, const char* key, bool& value)
{
    int p; if (!valueStart(json, key, p)) return false;
    if (json.substring(p, p + 4) == "true") { value = true; return true; }
    if (json.substring(p, p + 5) == "false") { value = false; return true; }
    return false;
}
bool readNumber(const String& json, const char* key, double& value)
{
    int p; if (!valueStart(json, key, p)) return false;
    const char* begin = json.c_str() + p; char* end = nullptr;
    value = strtod(begin, &end);
    return end != begin && std::isfinite(value) && (*end == ',' || *end == '}' || isspace(*end));
}
bool uintValue(const String& json, const char* key, uint32_t& value)
{
    double number; if (!readNumber(json, key, number) || number < 0 || number > 4294967295.0 || floor(number) != number) return false;
    value = static_cast<uint32_t>(number); return true;
}
bool parseConfig(const String& json, SensorConfig& c, String& error)
{
    static const char* forbidden[] = {"deviceId","deviceName","ssid","password","sequence","flashAddress","outboxCapacity","ackTimeout"};
    for (const char* key : forbidden) if (json.indexOf(String("\"") + key + "\"") >= 0) { error = String("protected field: ") + key; return false; }
    double version, number;
#define BAD(msg) do { error = msg; return false; } while (0)
    if (!readNumber(json,"version",version) || version != 1) BAD("version must be 1");
    if (!readBool(json,"events",c.eventDiagnosticsEnabled) || !readBool(json,"rawPressure",c.rawPressureDiagnosticsEnabled)) BAD("diagnostics values must be JSON booleans");
    if (!uintValue(json,"sampleIntervalMs",c.sampleIntervalMs) || !uintValue(json,"calibrationMs",c.calibrationMs) ||
        !uintValue(json,"minDurationMs",c.minDurationMs) || !uintValue(json,"maxDurationMs",c.maxDurationMs) ||
        !uintValue(json,"refractoryMs",c.refractoryMs) || !uintValue(json,"windowMs",c.bubbleActivityWindowMs)) BAD("integer field missing or invalid");
#define FLOAT_FIELD(key, member) if (!readNumber(json,key,number)) BAD(key " missing or invalid"); else c.member = static_cast<float>(number)
    FLOAT_FIELD("minTriggerDeltaPa",minTriggerDeltaPa); FLOAT_FIELD("noiseFactor",noiseFactor);
    FLOAT_FIELD("releaseFactor",releaseFactor); FLOAT_FIELD("baselineTrackingAlpha",baselineTrackingAlpha);
    FLOAT_FIELD("sendDeltaC",temperatureSendDeltaC);
#undef FLOAT_FIELD
    const char* validation = nullptr; if (!c.validate(validation)) BAD(validation);
#undef BAD
    return true;
}
String configJson(const SensorConfig& c)
{
    String s = "{\"version\":1,\"diagnostics\":{\"events\":"; s += c.eventDiagnosticsEnabled ? "true" : "false";
    s += ",\"rawPressure\":"; s += c.rawPressureDiagnosticsEnabled ? "true" : "false";
    s += "},\"pressure\":{\"sampleIntervalMs\":" + String(c.sampleIntervalMs) + ",\"calibrationMs\":" + String(c.calibrationMs);
    s += ",\"minTriggerDeltaPa\":" + String(c.minTriggerDeltaPa,6) + ",\"noiseFactor\":" + String(c.noiseFactor,6) + ",\"releaseFactor\":" + String(c.releaseFactor,6);
    s += ",\"minDurationMs\":" + String(c.minDurationMs) + ",\"maxDurationMs\":" + String(c.maxDurationMs) + ",\"refractoryMs\":" + String(c.refractoryMs);
    s += ",\"baselineTrackingAlpha\":" + String(c.baselineTrackingAlpha,6) + "},\"aggregation\":{\"windowMs\":" + String(c.bubbleActivityWindowMs);
    s += "},\"temperature\":{\"sendDeltaC\":" + String(c.temperatureSendDeltaC,6) + "}}"; return s;
}
String jsonEscape(const String& input) { String out; for (size_t i=0;i<input.length();++i) { const char c=input[i]; if (c == '\\' || c == '"') out += '\\'; out += c; } return out; }

const char* measurementStateName(MeasurementState state)
{
    switch (state) {
        case MeasurementState::IDLE: return "IDLE";
        case MeasurementState::RUNNING: return "RUNNING";
        case MeasurementState::PAUSED: return "PAUSED";
    }
    return "UNKNOWN";
}

String sensorIdString(const TemperatureSensorId& id)
{
    if (!id.isValid()) return "";
    static const char HEX_DIGITS[] = "0123456789ABCDEF";
    String value;
    value.reserve(23);
    for (size_t i = 0; i < TemperatureSensorId::SIZE; ++i) {
        if (i > 0) value += ':';
        value += HEX_DIGITS[(id.bytes[i] >> 4) & 0x0F];
        value += HEX_DIGITS[id.bytes[i] & 0x0F];
    }
    return value;
}

void appendError(String& json, bool& first, const String& error)
{
    if (!first) json += ',';
    json += '\"'; json += jsonEscape(error); json += '\"';
    first = false;
}

String statusJson(const MeasurementSession& session, const ServerClient& gateway,
    const TemperatureSensor& temperature, const PressureSensor& pressure)
{
    const bool temperatureAttempted = temperature.hasMeasurementAttempted();
    const bool temperatureReady = temperature.isLastMeasurementValid();
    const bool pressureAvailable = pressure.isAvailable();
    const bool pressureAttempted = pressure.hasReadAttempted();
    const bool pressureReady = pressure.isLastReadValid();

    String s = "{\"measurementState\":\"";
    s += measurementStateName(session.getState());
    s += "\",\"wifi\":{\"rssi\":"; s += String(WiFi.RSSI());
    s += "},\"gateway\":{\"connected\":"; s += gateway.isConnected() ? "true" : "false";
    s += ",\"registered\":"; s += gateway.isRegistered() ? "true" : "false";
    s += ",\"beerId\":\""; s += jsonEscape(gateway.finishedBeerId()); s += "\"}";

    s += ",\"temperature\":{\"attempted\":"; s += temperatureAttempted ? "true" : "false";
    s += ",\"ready\":"; s += temperatureReady ? "true" : "false";
    s += ",\"ambientId\":\""; s += sensorIdString(temperature.getAmbientSensorId()); s += "\"";
    s += ",\"beerId\":\""; s += sensorIdString(temperature.getBeerSensorId()); s += "\"";
    s += ",\"ambientC\":";
    if (temperatureReady) s += String(temperature.getAmbientTemperature(), 2); else s += "null";
    s += ",\"beerC\":";
    if (temperatureReady) s += String(temperature.getBeerTemperature(), 2); else s += "null";
    s += '}';

    s += ",\"pressure\":{\"available\":"; s += pressureAvailable ? "true" : "false";
    s += ",\"attempted\":"; s += pressureAttempted ? "true" : "false";
    s += ",\"ready\":"; s += pressureReady ? "true" : "false";
    s += ",\"pa\":";
    if (pressureReady) s += String(pressure.getPressurePa(), 2); else s += "null";
    s += ",\"error\":\"";
    if (!pressureAvailable) s += "INITIALIZATION_FAILED";
    else if (pressureAttempted && !pressureReady) s += pressure.getLastReadErrorName();
    s += "\"}";

    s += ",\"errors\":[";
    bool firstError = true;
    if (!temperature.getAmbientSensorId().isValid()) appendError(s, firstError, "Ambient-Temperatursensor nicht konfiguriert");
    if (!temperature.getBeerSensorId().isValid()) appendError(s, firstError, "Bier-Temperatursensor nicht konfiguriert");
    if (temperatureAttempted && !temperatureReady) appendError(s, firstError, "Temperatursensoren liefern keine vollständige gültige Messung");
    if (!pressureAvailable) appendError(s, firstError, "Drucksensor nicht verfügbar");
    else if (pressureAttempted && !pressureReady) appendError(s, firstError, String("Druckmessung fehlgeschlagen: ") + pressure.getLastReadErrorName());
    s += "]}";
    return s;
}
}

ConfigHttpServer::ConfigHttpServer(SensorConfigService& config, DeviceIdentity& identity,
    MeasurementSession& session, ServerClient& gateway,
    TemperatureSensor& temperatureSensor, PressureSensor& pressureSensor)
 : _config(config), _identity(identity), _session(session), _gateway(gateway),
   _temperatureSensor(temperatureSensor), _pressureSensor(pressureSensor), _server(PORT) {}

void ConfigHttpServer::begin() {}

void ConfigHttpServer::update()
{
    if (WiFi.status() != WL_CONNECTED) { if (_started) { _server.end(); _started = false; resetClient(); } return; }
    if (!_started) { _server.begin(); _started = true; Serial.print("CONFIG_HTTP_SERVER_STARTED,"); Serial.print(WiFi.localIP()); Serial.print(','); Serial.println(PORT); }
    if (!_client) { _client = _server.available(); if (_client) _lastActivityMs = millis(); return; }
    size_t budget = BYTE_BUDGET;
    while (budget-- && _client.available()) {
        char ch = static_cast<char>(_client.read()); _lastActivityMs = millis();
        if (!_headersComplete) { _headers += ch; if (_headers.length() > MAX_HEADERS) { respond(413,"application/json","{\"success\":false,\"error\":\"headers too large\"}"); return; }
            if (_headers.endsWith("\r\n\r\n")) { _headersComplete = true; int p = _headers.indexOf("Content-Length:"); if (p >= 0) _contentLength = static_cast<size_t>(_headers.substring(p + 15).toInt()); if (_contentLength > MAX_BODY) { respond(413,"application/json","{\"success\":false,\"error\":\"body too large\"}"); return; } }
        } else if (_body.length() < _contentLength) _body += ch;
    }
    if (_headersComplete && _body.length() >= _contentLength) processRequest();
    else if (millis() - _lastActivityMs > 1500) resetClient();
}

void ConfigHttpServer::processRequest()
{
    const int end = _headers.indexOf(" HTTP/"); const String request = end > 0 ? _headers.substring(0,end) : "";
    if (request == "GET /") respond(200,"text/html; charset=utf-8",CONFIG_PAGE);
    else if (request == "GET /api/config") respond(200,"application/json",configJson(_config.get()));
    else if (request == "GET /api/device") respond(200,"application/json",String("{\"deviceId\":\"") + jsonEscape(_identity.getDeviceId()) + "\",\"deviceName\":\"" + jsonEscape(_identity.getDeviceName()) + "\"}");
    else if (request == "GET /api/status") respond(200,"application/json",statusJson(_session, _gateway, _temperatureSensor, _pressureSensor));
    else if (request == "POST /api/config") {
        if (_session.getState() != MeasurementState::IDLE) { respond(409,"application/json","{\"success\":false,\"error\":\"measurement session must be IDLE\"}"); return; }
        SensorConfig candidate = SensorConfig::defaults(); String parseError;
        if (!parseConfig(_body,candidate,parseError)) { respond(400,"application/json",String("{\"success\":false,\"error\":\"") + jsonEscape(parseError) + "\"}"); return; }
        const char* error = nullptr; auto result = _config.update(candidate,error);
        if (result == SensorConfigUpdateResult::STORAGE_ERROR) respond(500,"application/json","{\"success\":false,\"error\":\"storage verification failed\"}");
        else respond(200,"application/json",result == SensorConfigUpdateResult::CHANGED ? "{\"success\":true,\"changed\":true}" : "{\"success\":true,\"changed\":false}");
    } else if (request == "PUT /api/device/name") {
        int p; if (!valueStart(_body,"name",p) || p >= static_cast<int>(_body.length()) || _body[p] != '"') { respond(400,"application/json","{\"success\":false,\"error\":\"name must be a string\"}"); return; }
        int q = _body.indexOf('"',p+1); if (q < 0) { respond(400,"application/json","{\"success\":false,\"error\":\"invalid name\"}"); return; }
        auto result = _identity.updateDeviceName(_body.substring(p+1,q));
        if (result == DeviceIdentity::NameUpdateResult::INVALID) respond(400,"application/json","{\"success\":false,\"error\":\"name is empty, invalid, or too long\"}");
        else if (result == DeviceIdentity::NameUpdateResult::STORAGE_ERROR) respond(500,"application/json","{\"success\":false,\"error\":\"storage verification failed\"}");
        else { if (result == DeviceIdentity::NameUpdateResult::CHANGED) _gateway.requestReconnect(); respond(200,"application/json",result == DeviceIdentity::NameUpdateResult::CHANGED ? "{\"success\":true,\"changed\":true}" : "{\"success\":true,\"changed\":false}"); }
    } else respond(404,"application/json","{\"success\":false,\"error\":\"not found\"}");
}

void ConfigHttpServer::respond(int status, const char* type, const String& body)
{
    _client.print("HTTP/1.1 "); _client.print(status); _client.println(status==200?" OK":status==400?" Bad Request":status==409?" Conflict":status==413?" Payload Too Large":status==500?" Internal Server Error":" Not Found");
    _client.print("Content-Type: "); _client.println(type); _client.print("Content-Length: "); _client.println(body.length()); _client.println("Connection: close\r\n"); _client.print(body); _client.stop(); resetClient();
}
void ConfigHttpServer::resetClient() { if (_client) _client.stop(); _client = WiFiClient(); _headers=""; _body=""; _contentLength=0; _headersComplete=false; }
