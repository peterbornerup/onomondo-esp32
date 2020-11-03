#pragma once

#define SIM800L_IP5306_VERSION_20190610
#include "utilities.h"
// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to the module)
#define SerialAT Serial1

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800    // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024  // Set RX buffer to 1Kb

#define DUMP_AT_COMMANDS
#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
#endif

#include <Wire.h>

#include "LEDHandler.h"

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 60          /* Time ESP32 will go to sleep (in seconds) */

class Onomondo {
   public:
    //initializes the modem...
    Onomondo(LEDHandler *leds = NULL);

    ~Onomondo();

    //creates TCP
    uint8_t connect(char *server, int port);

    uint8_t writeTCP(const char *data, const int &len);

    //closes connection
    uint8_t disconnect();

    uint8_t sleep();

    uint16_t getSignalQuality() {
        //
        static uint16_t signal = 99;

        if (modem_) {
            uint61_t tmp = modem_->getSignalQuality();
            // if 99 modem failed to get signal quality
            // polling again seems to fix the problem.
            if (tmp == 99) {
                return modem_->getSignalQuality();
            }
        }
        //error: modem not initialized
        return -1;
    }

   private:
    LEDHandler *leds_ = NULL;

    const char *apn = "onomondo";  // Your APN
#ifdef DUMP_AT_COMMANDS
    StreamDebugger *debugger_ = NULL;
    TinyGsm *modem_ = NULL;
#else
    TinyGsm *modem_ = NULL;
#endif

    bool TCPConnectionOK_ = false;

    void _setup();

    void _turnOffNetlight() {
        SerialMon.println("Turning off SIM800 Red LED...");
        modem_->sendAT("+CNETLIGHT=0");
    }

    void _turnOnNetlight() {
        SerialMon.println("Turning on SIM800 Red LED...");
        modem_->sendAT("+CNETLIGHT=1");
    }

#define RESPONSE_BUFFER_SIZE 255
    //for modem response
    char resp_[RESPONSE_BUFFER_SIZE];
    bool sendATExpectOKBase(const char *cmd);
    bool sendATExpectOK(const char *cmd) {
        return sendATExpectOKBase(cmd);
    }
    bool sendATOKResp(const char *cmd, const char *resp);
    bool sendATExpectResp(const char *cmd, const char *resp) {
        sendATExpectOKBase(cmd);
        //check if resp is in the response buffer...

        //SerialMon.println("comparing:"); SerialMon.println(resp_); SerialMon.println(resp);

        return strstr(resp_, resp) != NULL;
    }
};
