#include "storage/WifiCredentialStore.h"

#include <Arduino.h>

namespace
{
constexpr char WIFI_SSID_KEY[] =
    "wifi_ssid";

constexpr char WIFI_PASSWORD_KEY[] =
    "wifi_password";
}

WifiCredentialStore::WifiCredentialStore(
    FlashStorage& storage
)
    : _storage(storage)
{
}

bool WifiCredentialStore::begin()
{
    _initialized = true;

    Serial.println(
        "WifiCredentialStore: ready."
    );

    return true;
}

bool WifiCredentialStore::hasCredentials() const
{
    if (!_initialized) {
        return false;
    }

    return _storage.exists(
        WIFI_SSID_KEY
    );
}

WifiCredentials WifiCredentialStore::load() const
{
    WifiCredentials credentials;

    if (!_initialized) {
        Serial.println(
            "WifiCredentialStore: not initialized."
        );

        return credentials;
    }

    if (!_storage.getString(
        WIFI_SSID_KEY,
        credentials.ssid
    )) {
        Serial.println(
            "WifiCredentialStore: no SSID stored."
        );

        return credentials;
    }

    if (!_storage.getString(
        WIFI_PASSWORD_KEY,
        credentials.password
    )) {
        Serial.println(
            "WifiCredentialStore: no password stored."
        );

        return WifiCredentials{};
    }

    Serial.print(
        "WifiCredentialStore: credentials loaded for "
    );
    Serial.println(
        credentials.ssid
    );

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
        Serial.println(
            "WifiCredentialStore: invalid credentials."
        );

        return false;
    }

    if (!_storage.setString(
        WIFI_SSID_KEY,
        credentials.ssid
    )) {
        Serial.println(
            "WifiCredentialStore: failed to save SSID."
        );

        return false;
    }

    if (!_storage.setString(
        WIFI_PASSWORD_KEY,
        credentials.password
    )) {
        Serial.println(
            "WifiCredentialStore: failed to save password."
        );

        return false;
    }

    Serial.print(
        "WifiCredentialStore: credentials saved for "
    );
    Serial.println(
        credentials.ssid
    );

    return true;
}

bool WifiCredentialStore::clear()
{
    if (!_initialized) {
        return false;
    }

    _storage.remove(
        WIFI_SSID_KEY
    );

    _storage.remove(
        WIFI_PASSWORD_KEY
    );

    Serial.println(
        "WifiCredentialStore: credentials cleared."
    );

    return true;
}