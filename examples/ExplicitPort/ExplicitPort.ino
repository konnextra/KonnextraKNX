/*
 * ExplicitPort — naming the serial port the transceiver is wired to.
 *
 * Without a second argument the library uses this board's default KNX port (Serial1 on
 * most boards). Name a port when the transceiver sits somewhere else, or when the board
 * has no second port at all, like the Uno, where the only hardware UART is the one the
 * USB console shares. Note what that costs on such a board: Serial belongs to KNX from
 * here on, so there is no serial monitor to print to.
 */

#include <Konnextra.h>

Konnextra knx("1.1.5", Serial);         // the transceiver is on the one hardware UART
KnxLight  lamp(knx, "0/1/1", "0/3/0");

unsigned long lastToggle = 0;

void setup() {
    knx.begin();                        // opens Serial at 19200 8E1
}

void loop() {
    knx.loop();

    if (millis() - lastToggle >= 5000) {
        lastToggle = millis();
        lamp.toggle();
    }
}
