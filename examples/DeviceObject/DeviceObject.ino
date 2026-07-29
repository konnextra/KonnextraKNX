/*
 * DeviceObject — the usual way to use the library.
 *
 * Create one object per thing on the bus, command it with named methods, and
 * register a callback that fires when the value changes on the bus.
 */

#include <Konnextra.h>

Konnextra knx("1.1.5");                 // this device's KNX address
KnxLight  lamp(knx, "0/1/1", "0/3/0");  // command address, status address

void onLampChanged(bool on);            // defined below

unsigned long lastToggle = 0;

void setup() {
    knx.begin();
    lamp.onUpdate(onLampChanged);       // called when the lamp changes on the bus
}

void loop() {
    knx.loop();                         // receives telegrams, fires callbacks

    // Toggle every 5 s. Never use delay() for this: it stalls knx.loop(),
    // so status telegrams arrive late or are missed entirely.
    if (millis() - lastToggle >= 5000) {
        lastToggle = millis();
        lamp.toggle();                  // flips relative to the real bus state
    }
}

void onLampChanged(bool on) {
    // react to the light's new state
}
