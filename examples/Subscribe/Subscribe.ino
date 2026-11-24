/*
 * Subscribe — receive values from any group address, without keeping an object.
 *
 * The counterpart to knx.send(): name the address and its datapoint type once,
 * and your function is called whenever a value arrives there. Handy for a
 * display that only shows what is happening on the bus.
 */

#include <KonnextraKNX.h>

KonnextraKNX knx("1.1.5");

void onLightStatus(const KnxTelegram& telegram);   // defined below
void onTemperature(const KnxTelegram& telegram);

void setup() {
    Serial.begin(115200);
    knx.begin();
    knx.subscribe("0/3/0", KnxDpt::DPT1, onLightStatus);   // an on/off status
    knx.subscribe("0/4/2", KnxDpt::DPT9, onTemperature);   // a temperature
}

void loop() {
    knx.loop();                          // receives telegrams, calls your functions
}

void onLightStatus(const KnxTelegram& telegram) {
    Serial.print("Light is ");
    Serial.println(telegram.value().asBool() ? "on" : "off");
}

void onTemperature(const KnxTelegram& telegram) {
    Serial.print("Temperature: ");
    Serial.print(telegram.value().asFloat());
    Serial.println(" C");
}
