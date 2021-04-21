#include <ArduinoJson.h>
#include <Temperature_LM75_Derived.h>
#include <Ticker.h>

#include "Onomondo.h"
#include "settings.h"

LEDHandler leds;
Ticker ticker;

// modem (and some LED control)
Onomondo *onomondo = NULL;

Generic_LM75_9_to_12Bit tempSensor(&Wire, 0x48);

esp_sleep_wakeup_cause_t wakeup_reason;

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */

#define LED_GPIO 13
#define LED_ON HIGH
#define LED_OFF LOW

#define V_BATT 36
bool debug = true;

void updateLeds() {
    // callback for the led handler. This will toggle the leds based on the current state.
    leds.periodicUpdate();
}
// callback for touch input
void callbackTouchpin() {
    // good for debug. Power off device if pressed during runtime.
    //poweroff();
}

float getBatteryVoltage() {
    int rawAnalog = analogRead(V_BATT);
    return (float)rawAnalog / 4096.0 * 3.3 * 1.5;  //convert to volts
}
// Temperature sensor

void initTempSensor() {
    Wire.begin();
    tempSensor.setResolution(Generic_LM75_9_to_12Bit::Resolution_12_bits);
}

void setup() {
    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LED_ON);
    leds.changeState(STATE_ON);

    // the the callback to a periodic timer
    ticker.attach_ms(20, updateLeds);

    //input for battery measurement
    pinMode(V_BATT, INPUT);

    //Setup interrupt on Touch Pad 3 (GPIO15) (for wake up)
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    //esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, 1);
    touchAttachInterrupt(T9, callbackTouchpin, 40);
    esp_sleep_enable_touchpad_wakeup();

    wakeup_reason = esp_sleep_get_wakeup_cause();
    //go back to sleep if the battery wwas just connected.
    if (!(wakeup_reason == ESP_SLEEP_WAKEUP_TIMER || wakeup_reason == ESP_SLEEP_WAKEUP_TOUCHPAD)) {
        delay(300);
        esp_deep_sleep_start();
    }

    //handle low battery
    if (false && getBatteryVoltage() < 3.2) {
        leds.changeState(STATE_ERROR);
        delay(400);
        leds.changeState(STATE_POWER_OFF);
        poweroff();
    }

    //init modem
    onomondo = new Onomondo(statusCallback);

    //temperature sensor
    initTempSensor();
}

void statusCallback(modemStates state) {
    leds.changeState(state);
}

void loop() {
    //connect to the specified port
    bool connected = onomondo->connect("1.2.3.4", 4321);

    //json document to transmit data
    DynamicJsonDocument doc(400);

    //failure to connect... -> go back to sleep
    if (!connected) {
        error();
    }

    //send some payload 5 times..
    uint8_t TCPOk;
    for (int i = 0; i < 5; i++) {
        //////////////////////////////////////////////
        ////HERE GOES WHATEVER SHOULD BE TRANSMITTED//
        //////////////////////////////////////////////
        doc["battery"] = getBatteryVoltage();
        doc["signal"] = onomondo->getSignalQuality();
        doc["temperature"] = (float)tempSensor.readTemperatureC();  //add the measurement to the document

        //serialize the json doc
        char serializedJson[200];
        int size = serializeJson(doc, serializedJson, sizeof(serializedJson));

        //assuming everything is OK we can now transmit
        //easy with Onomondo -> everything is handled on the serverside.
        if (!onomondo->writeTCP(serializedJson, size)) {
            error();
        }
    }

    //disconnect
    if (connected)
        onomondo->disconnect();
    //flash ledsssss
    leds.changeState(STATE_DONE);
    delay(200);
    leds.changeState(STATE_DATA_OK);
    delay(200);
    leds.changeState(STATE_NETWRK_OK);
    delay(200);
    leds.changeState(STATE_ON);

    poweroff();
}

void poweroff() {
    if (onomondo)
        onomondo->sleep();
    leds.changeState(STATE_POWER_OFF);
    esp_deep_sleep_start();
}

void error() {
    leds.changeState(STATE_ERROR);
    // add a small delay
    delay(3000);

    poweroff();
}