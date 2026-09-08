#include "network/WifiCredentialStore.h"

#include <Arduino.h>
#include <kvstore_global_api.h>

namespace
{
constexpr char WIFI_SSID_KEY[] = "wifi_ssid";
constexpr char WIFI_PASSWORD_KEY[] = "wifi_password";

constexpr int KV_SUCCESS = 0;
}

bool WifiCredentialStore::begin()
{
    _initialized = true;

    Serial.println("WifiCredentialStore: ready.");

    return true;
}

bool WifiCredentialStore::hasCredentials() const
{
    if (!_initialized) {
        return false;
    }

    kv_info_t info;

    const int result = kv_get_info(
        WIFI_SSID_KEY,
        &info
    );

    return result == KV_SUCCESS && info.size > 0;
}

WifiCredentials WifiCredentialStore::load() const
{
    WifiCredentials credentials;

    if (!_initialized) {
        Serial.println("WifiCredentialStore: not initialized.");
        return credentials;
    }

    kv_info_t ssidInfo;
    kv_info_t passwordInfo;

    if (kv_get_info(WIFI_SSID_KEY, &ssidInfo) != KV_SUCCESS) {
        Serial.println("WifiCredentialStore: no SSID stored.");
        return credentials;
    }

    if (kv_get_info(WIFI_PASSWORD_KEY, &passwordInfo) != KV_SUCCESS) {
        Serial.println("WifiCredentialStore: no password stored.");
        return credentials;
    }

    char ssidBuffer[64] = {};
    char passwordBuffer[128] = {};

    if (ssidInfo.size >= sizeof(ssidBuffer)) {
        Serial.println("WifiCredentialStore: stored SSID is too long.");
        return credentials;
    }

    if (passwordInfo.size >= sizeof(passwordBuffer)) {
        Serial.println("WifiCredentialStore: stored password is too long.");
        return credentials;
    }

    size_t actualSize = 0;

    int result = kv_get(
        WIFI_SSID_KEY,
        ssidBuffer,
        sizeof(ssidBuffer) - 1,
        &actualSize
    );

    if (result != KV_SUCCESS) {
        Serial.println("WifiCredentialStore: failed to load SSID.");
        return credentials;
    }

    result = kv_get(
        WIFI_PASSWORD_KEY,
        passwordBuffer,
        sizeof(passwordBuffer) - 1,
        &actualSize
    );

    if (result != KV_SUCCESS) {
        Serial.println("WifiCredentialStore: failed to load password.");
        return credentials;
    }

    credentials.ssid = ssidBuffer;
    credentials.password = passwordBuffer;

    Serial.print("WifiCredentialStore: credentials loaded for ");
    Serial.println(credentials.ssid);

    return credentials;
}

bool WifiCredentialStore::save(
    const WifiCredentials& credentials
)
{
    if (!_initialized) {
        return false;
    }

    if (!credentials.isValid()) {
        Serial.println("WifiCredentialStore: invalid credentials.");
        return false;
    }

    int result = kv_set(
        WIFI_SSID_KEY,
        credentials.ssid.c_str(),
        credentials.ssid.length() + 1,
        0
    );

    if (result != KV_SUCCESS) {
        Serial.println("WifiCredentialStore: failed to save SSID.");
        return false;
    }

    result = kv_set(
        WIFI_PASSWORD_KEY,
        credentials.password.c_str(),
        credentials.password.length() + 1,
        0
    );

    if (result != KV_SUCCESS) {
        Serial.println("WifiCredentialStore: failed to save password.");
        return false;
    }

    Serial.print("WifiCredentialStore: credentials saved for ");
    Serial.println(credentials.ssid);

    return true;
}

bool WifiCredentialStore::clear()
{
    if (!_initialized) {
        return false;
    }

    kv_remove(WIFI_SSID_KEY);
    kv_remove(WIFI_PASSWORD_KEY);

    Serial.println("WifiCredentialStore: credentials cleared.");

    return true;
}