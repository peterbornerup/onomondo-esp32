#pragma once

enum modemStates { STATE_POWER_OFF,
                   STATE_ON,
                   STATE_NETWORK,
                   STATE_NETWRK_OK,
                   STATE_DATA,
                   STATE_DATA_OK,
                   STATE_TRAFFIC,
                   STATE_DONE,
                   STATE_LOW_BATTERY,
                   STATE_ERROR
};

class LEDHandler {
   public:
    LEDHandler();
    ~LEDHandler();

    void changeState(const modemStates& newState);

    void setBlinkSpeed(const int ms) {
        if (ms < 1) {
            blinkWaitMs_ = 1;
        } else {
            blinkWaitMs_ = ms;
        }
    }

    void periodicUpdate() {
        ledUpdateCallback_();
    }

   private:
    int blinkWaitMs_ = 100;
    unsigned long lastUpdate_ = 0;
    bool flashState_ = false;
    bool rapidFlash_ = false;
    modemStates state_ = STATE_POWER_OFF;

    char leds[6];

    void ledUpdateCallback_();
};
