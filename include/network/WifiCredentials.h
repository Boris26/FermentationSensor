#pragma once

#include <Arduino.h>

struct WifiCredentials
{
    String ssid;
    String password;

    bool isValid() const
    {
        return !ssid.isEmpty();
    }
};