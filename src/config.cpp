#include "main.h"
#include "config.h"

void configurePins()
{
    pinMode(LedWifi, OUTPUT);
    pinMode(LedBluetooth, OUTPUT);
    pinMode(Ledheartbeat, OUTPUT);
    pinMode(LedFireBase, OUTPUT);

    digitalWrite(LedWifi, LOW);
    digitalWrite(LedBluetooth, LOW);
    digitalWrite(Ledheartbeat, LOW);
    digitalWrite(LedFireBase, LOW);
}