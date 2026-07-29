/*
 * StatelessSend — a one-off value to any group address, without keeping an object.
 *
 * There is no status callback on this path; it only sends. Each Dpt*() factory
 * accepts only the data its datapoint expects, so a wrong value type is a compile
 * error rather than a bad telegram on the bus.
 */

#include <Konnextra.h>

Konnextra knx("1.1.5");

void setup() {
    knx.begin();
    knx.send("0/1/1", Dpt1(true));      // an on/off value
    knx.send("0/4/2", Dpt9(21.5f));     // a temperature, as a floating-point value
}

void loop() {
    knx.loop();
}
