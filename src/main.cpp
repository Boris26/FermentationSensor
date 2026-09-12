#include <Arduino.h>

#include "app/FermentationSensorApplication.h"

FermentationSensorApplication application;

void setup()
{
    application.begin();
}

void loop()
{
    application.update();
}
