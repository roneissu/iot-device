#include "commands.h"

int blinkLed(int ledTimes, int delaySec)
{
    if (!ledTimes || !delaySec)
        return 2;

    for (int j = 0; j < ledTimes; j++)
    {
        delay(delaySec);
        digitalWrite(1, LOW);
        delay(delaySec);
        digitalWrite(1, HIGH);
    }
    return 1;
}

float getButtonValue()
{
    return 36.5;
}