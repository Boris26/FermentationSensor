#include "storage/WifiCredentialStore.h"

#include <Arduino.h>

namespace
{
constexpr char WIFI_CONFIG_KEY[] =
    "wifi_config";

constexpr char LEGACY_WIFI_SSID_KEY[] =
    "wifi_ssid";

constexpr char LEGACY_WIFI_PASSWORD_KEY[] =
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

    if (!_storage.exists(WIFI_CONFIG_KEY)) {
        WifiCredentials legacyCredentials;

        if (
            _storage.getString(
                LEGACY_WIFI_SSID_KEY,
                legacyCredentials.ssid
            ) &&
            _storage.getString(
                LEGACY_WIFI_PASSWORD_KEY,
                legacyCredentials.password
            )
        ) {
            Serial.println(
                "WifiCredentialStore: migrating legacy credentials."
            );

            if (!save(legacyCredentials)) {
                Serial.println(
                    "WifiCredentialStore: legacy migration failed."
                );

                return false;
            }

            const bool legacySsidRemoved =
                removeIfPresent(LEGACY_WIFI_SSID_KEY);

            const bool legacyPasswordRemoved =
                removeIfPresent(LEGACY_WIFI_PASSWORD_KEY);

            if (
                !legacySsidRemoved ||
                !legacyPasswordRemoved
            ) {
                Serial.println(
                    "WifiCredentialStore: failed to remove legacy credentials."
                );

                return false;
            }
        }
    }

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

    const bool configRemoved =
        removeIfPresent(WIFI_CONFIG_KEY);

    const bool legacySsidRemoved =
        removeIfPresent(LEGACY_WIFI_SSID_KEY);

    const bool legacyPasswordRemoved =
        removeIfPresent(LEGACY_WIFI_PASSWORD_KEY);

    if (
        !configRemoved ||
        !legacySsidRemoved ||
        !legacyPasswordRemoved
    ) {
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


bool WifiCredentialStore::removeIfPresent(
    const char* key
)
{
    return
        !_storage.exists(key) ||
        _storage.remove(key);
}
