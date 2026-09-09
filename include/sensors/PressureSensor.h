#pragma once

class PressureSensor
{
public:
    void begin();
    void update();

    bool isAvailable() const;
    float getPressurePa() const;

private:
    bool _available = false;
    float _pressurePa = 0.0f;
};