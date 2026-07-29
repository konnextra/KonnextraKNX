/*
 * CustomKnxObject — a datapoint that has no dedicated device class.
 *
 * Give the generic KnxObject a group address and a datapoint type once, then send
 * with write() and receive with onUpdate(). Here a 16-bit counter (DPT 7).
 */

#include <Konnextra.h>

Konnextra knx("1.1.5");
KnxObject counter(knx, "0/5/0", KnxDpt::DPT7);   // 16-bit unsigned value

void onCounter(const KnxValue& value);           // defined below

void setup() {
    knx.begin();
    counter.onUpdate(onCounter);
    counter.write(Dpt7(1000));          // send a value
}

void loop() {
    knx.loop();
}

void onCounter(const KnxValue& value) {
    uint16_t count = value.asU16();     // read the received value as its type
}
