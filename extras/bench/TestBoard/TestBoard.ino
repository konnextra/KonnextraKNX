/**
 * @name TestBoard.ino
 * @date 24.11.2026
 * @authors Florian Wiesner
 * @details Sketch for the soldered test board (XIAO ESP32-C6 + BCU light, two buttons, two LEDs).
 *
 *          Right button: short = on,  hold = dim up,   release = stop.
 *          Left button:  short = off, hold = dim down, release = stop.
 *          Both LEDs show whether the lamp is on.
 *
 *          The LEDs follow the actuator's status address. Until a first status has arrived they
 *          follow the switching commands seen on the bus instead, because the recordings of
 *          22.11. contain no status telegram at all. Serial says which source set them.
 */

#include <Arduino.h>
#include <KonnextraKNX.h>

//---- Group addresses ----
#define GA_SWITCH           "0/1/1"
#define GA_DIM              "0/0/1"
#define GA_STATUS           "0/3/0"   // from the thesis firmware (0738a5f, src/main.cpp)

//---- Pins ----
#define PIN_SAVE            D0
#define PIN_RESET           D1
#define PIN_LED_1           D2
#define PIN_BUTTON_LEFT     D3        // swap the two button pins if they are mirrored
#define PIN_BUTTON_RIGHT    D8
#define PIN_LED_2           D9
#define PIN_KNX_TX          D6
#define PIN_KNX_RX          D7

//---- Timing ----
#define DEBOUNCE_MS         30
#define LONG_PRESS_MS       400       // as in the thesis firmware
#define RETRY_BEGIN_MS      5000

HardwareSerial knxPort(1);
KonnextraKNX knx("1.1.5", knxPort, KnxTransceiver::Ncn5130);
KnxDimmLight lamp(knx, GA_SWITCH, GA_DIM, GA_STATUS);

struct Button {
    uint8_t  pin;
    bool     isRight;
    bool     lastRaw;       // last raw level, true = pressed
    bool     pressed;       // debounced state
    bool     _dimming;      // long press is active
    uint32_t ts_change;
    uint32_t ts_press;
};

Button buttonLeft  = { PIN_BUTTON_LEFT,  false, false, false, false, 0, 0 };
Button buttonRight = { PIN_BUTTON_RIGHT, true,  false, false, false, 0, 0 };

bool     _busUp      = false;
bool     _statusSeen = false;         // true once the actuator has reported its status
uint32_t ts_lastBegin = 0;

void showLamp(bool on, const char* source);
void showCommand(bool on);
void handleButton(Button& button);
void onLampStatus(bool on);
void onSwitchCommand(const KnxTelegram& telegram);
void onBusPowerFail();

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);         // without a USB host, printing must never stall loop()

    pinMode(PIN_LED_1, OUTPUT);
    pinMode(PIN_LED_2, OUTPUT);
    pinMode(PIN_BUTTON_LEFT, INPUT_PULLUP);
    pinMode(PIN_BUTTON_RIGHT, INPUT_PULLUP);

    // LED self-test, also gives the USB monitor a moment to connect.
    digitalWrite(PIN_LED_1, HIGH);
    digitalWrite(PIN_LED_2, HIGH);
    delay(1000);
    digitalWrite(PIN_LED_1, LOW);
    digitalWrite(PIN_LED_2, LOW);

    knx.setDebugMode(KnxLog::Basic);  // KnxLog::Deep for every detail
    knx.setStatusPins(PIN_RESET, PIN_SAVE);
    knx.onBusPowerFail(onBusPowerFail);
    knxPort.setPins(PIN_KNX_RX, PIN_KNX_TX);

    lamp.onUpdate(onLampStatus);
    knx.subscribe(GA_SWITCH, KnxDpt::DPT1, onSwitchCommand);   // e.g. the wall button

    _busUp = knx.begin();
    ts_lastBegin = millis();
}

void loop() {
    if (!_busUp) {
        if (millis() - ts_lastBegin >= RETRY_BEGIN_MS) {
            _busUp = knx.begin();
            ts_lastBegin = millis();
        }
        return;
    }

    knx.loop();
    handleButton(buttonLeft);
    handleButton(buttonRight);
}

// Debounces one button. Short press switches on release; holding dims until release.
void handleButton(Button& button) {
    const uint32_t now = millis();
    const bool raw = digitalRead(button.pin) == LOW;

    if (raw != button.lastRaw) {
        button.lastRaw = raw;
        button.ts_change = now;
    }

    if (raw != button.pressed && now - button.ts_change >= DEBOUNCE_MS) {
        button.pressed = raw;
        if (raw) {
            button.ts_press = now;
        }
        else if (button._dimming) {
            button._dimming = false;
            lamp.stopDimming();
        }
        else {
            const bool sent = button.isRight ? lamp.on() : lamp.off();
            if (sent) showCommand(button.isRight);
        }
    }

    if (button.pressed && !button._dimming && now - button.ts_press >= LONG_PRESS_MS) {
        button._dimming = true;
        const bool sent = button.isRight ? lamp.brighter() : lamp.darker();
        if (sent && button.isRight) showCommand(true);   // dimming up switches the lamp on
    }
}

void showLamp(bool on, const char* source) {
    digitalWrite(PIN_LED_1, on ? HIGH : LOW);
    digitalWrite(PIN_LED_2, on ? HIGH : LOW);
    Serial.print("[led] ");
    Serial.print(on ? "on" : "off");
    Serial.print(" (");
    Serial.print(source);
    Serial.println(")");
}

// A command only says what was asked for, so a real status always takes precedence.
void showCommand(bool on) {
    if (!_statusSeen) showLamp(on, "command, no status received yet");
}

void onLampStatus(bool on) {
    _statusSeen = true;
    showLamp(on, "status " GA_STATUS);
}

void onSwitchCommand(const KnxTelegram& telegram) {
    showCommand(telegram.value().asBool());
}

void onBusPowerFail() {
    Serial.println("[power] bus power failing (save pin low)");
}
