#include "storage/WifiCredentialStore.h"

#include <Arduino.h>

namespace
{
constexpr char WIFI_CONFIG_KEY[] =
    "wifi_config";

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
    return load().isValid();
}


WifiCredentials WifiCredentialStore::load() const
{
    if (!_initialized) {
        Serial.println(
            "WifiCredentialStore: not initialized."
        );

        return WifiCredentials{};
    }

    if (!_storage.exists(WIFI_CONFIG_KEY)) {
        return WifiCredentials{};
    }

    StoredWifiConfiguration stored;

    if (
        !_storage.getBytes(
            WIFI_CONFIG_KEY,
            &stored,
            sizeof(stored)
        )
    ) {
        Serial.println(
            "WifiCredentialStore: failed to load credentials."
        );

        return WifiCredentials{};
    }

    if (
        stored.version != STORAGE_VERSION ||
        stored.ssid[MAX_SSID_LENGTH] != '\0' ||
        stored.password[MAX_PASSWORD_LENGTH] != '\0'
    ) {
        Serial.println(
            "WifiCredentialStore: stored credentials are invalid."
        );

        return WifiCredentials{};
    }

    WifiCredentials credentials;
    credentials.ssid = String(stored.ssid);
    credentials.password = String(stored.password);

    if (!isValid(credentials)) {
        Serial.println(
            "WifiCredentialStore: stored credential lengths are invalid."
        );

        return WifiCredentials{};
    }

    Serial.print(
        "WifiCredentialStore: credentials loaded for "
    );
    Serial.println(credentials.ssid);

    return credentials;
}


bool WifiCredentialStore::save(
    const WifiCredentials& credentials
)
{
    if (
        !_initialized ||
        !isValid(credentials)
    ) {
        Serial.println(
            "WifiCredentialStore: invalid credentials."
        );

        return false;
    }

    StoredWifiConfiguration stored;

    credentials.ssid.toCharArray(
        stored.ssid,
        sizeof(stored.ssid)
    );

    credentials.password.toCharArray(
        stored.password,
        sizeof(stored.password)
    );

    if (
        !_storage.setBytes(
            WIFI_CONFIG_KEY,
            &stored,
            sizeof(stored)
        )
    ) {
        Serial.println(
            "WifiCredentialStore: failed to save credentials."
        );

        return false;
    }

    Serial.print(
        "WifiCredentialStore: credentials saved for "
    );
    Serial.println(credentials.ssid);

    return true;
}


bool WifiCredentialStore::clear()
{
    if (!_initialized) {
        return false;
    }

    if (_storage.exists(WIFI_CONFIG_KEY) && !_storage.remove(WIFI_CONFIG_KEY)) {
        Serial.println(
            "WifiCredentialStore: failed to clear credentials."
        );

        return false;
    }

    Serial.println(
        "WifiCredentialStore: credentials cleared."
    );

    return true;
}


bool WifiCredentialStore::isValid(
    const WifiCredentials& credentials
) const
{
    return
        credentials.isValid() &&
        credentials.ssid.length() <= MAX_SSID_LENGTH &&
        credentials.password.length() <= MAX_PASSWORD_LENGTH;
}
