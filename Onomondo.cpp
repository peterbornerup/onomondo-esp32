#include "Onomondo.h"

#define DEFAULT_DELAY 300

Onomondo::Onomondo(void (*statusCallback)(modemStates)) {
    // Set console baud rate
    SerialMon.begin(115200);

#ifdef DUMP_AT_COMMANDS  //yeee.
    debugger_ = new StreamDebugger(SerialAT, SerialMon);
    modem_ = new TinyGsm(*debugger_);
#else
    modem_ = new TinyGsm(SerialAT);
#endif  // DUMP_AT_COMMANDS

    cb_ = statusCallback;

    // PMU settings (specific to the ESP32 board used)
    Wire.beginTransmission(IP5306_ADDR);
    int bytes = Wire.write(IP5306_REG_SYS_CTL0);

    bytes = Wire.write(0x37);  // Set bit1: 1 enable 0 disable boost keep on
    Wire.endTransmission() == 0;

    //init GPIO and start modem
    _setup();

    delay(10);

    // Set GSM module baud rate and UART pins
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

    delay(10);
}

Onomondo::~Onomondo() {
}

uint8_t Onomondo::connect(char *server, int port) {
    bool success = false;

    success = modem_->init();

    //initialize the modem
    if (!success) {
        success = modem_->restart();

        if (!success) {
            return false;
        }
    }

    //we now register on the network
    if (cb_)
        cb_(STATE_NETWORK);

    if (!modem_->waitForNetwork(40000L)) {
        // might be problems with missing whitelist
        // clear forbidden network liest...
        DB("Failed to connect. Clearing the forbidden network list (FPLMN)...");

        sendATExpectOK("+CRSM=214,28539,0,0,12,\"FFFFFFFFFFFFFFFFFFFFFFFF\"");

        //retry
        if (!modem_->waitForNetwork(60000L)) {
            Db("Failed to register to the network");

            //clear it for next run.
            Db("Clearing the forbidden network list (FPLMN)... :) ");
            sendATExpectOK("+CRSM=214,28539,0,0,12,\"FFFFFFFFFFFFFFFFFFFFFFFF\"");

            return 0;
        }
    }

    if (!modem_->isNetworkConnected()) {
        DB("not connected to network...");
        return 0;
    }

    //network OK
    if (cb_)
        cb_(STATE_NETWRK_OK);

    //delay for visual effect :p
    delay(DEFAULT_DELAY);

    if (cb_)
        cb_(STATE_DATA);

    //set apn
    int n = 0;
    char apnString[50];
    sprintf(apnString, "+CSTT=\"%s\"", APN);

    while (!(success = sendATExpectOK(apnString)) && n++ < 6) {
        DB("Retrying... cstt");
        delay(1000);
    }
    if (!success) {
        DB("failed to set APN: onomondo (+CSTT)");
        return 0;
    }

    delay(DEFAULT_DELAY * 3);

    if (CPOL == 1) {
        this->printCPOL();
    }

    delay(DEFAULT_DELAY * 3);
    //status
    //WE MUST WAIT FOR A "1" RESPONSE
    while (!(success = sendATOKResp("+CGATT?", "1")) && n++ < 15) {
        DB("CGATT>>>");
        delay(1000);
    }
    if (!success) {
        Db("Failed to attach to packet domain service (+CGATT)");
        return 0;
    }

    delay(DEFAULT_DELAY * 6);

    //bring up GPRS
    while (!(success = sendATExpectOK("+CIICR")) && n++ < 10) {
        delay(2000);
    }
    if (!success) {
        DB("Failed to bring up GPRS. Maybe the data is blocked by onomondo? (+CIICR)");
        return 0;
    }

    delay(DEFAULT_DELAY);

    //ip adress //must be read for some reason. See SIM800 datasheet
    modem_->sendAT("+CIFSR");
    //wait for response
    // delay(300);
    while (!SerialAT.available())
        ;
    while (SerialAT.available()) {
        SerialMon.write(SerialAT.read());
    }

    char buffer[50];

    //initialize TCP Stack with server and port // can be changed for UDP
    sprintf(buffer, "+CIPSTART=\"TCP\",\"%s\",\"%u\"", server, port);

    delay(DEFAULT_DELAY);
    //Init tcp stack on modem.
    while (!(success = sendATExpectOK(buffer)) && n++ < 4) {
        DB("Retrying... tcp initialization");
        delay(1000);
    }
    if (!success) {
        DB("Modem failed to initialize TCP stack..");
        return 0;
    }

    TCPConnectionOK_ = true;

    if (cb_)
        cb_(STATE_DATA_OK);

    return 1;
}

uint8_t Onomondo::writeTCP(const char *data, const int &len) {
    if (!TCPConnectionOK_)
        return 0;

    bool success = false;
    int i = 0;

    sendATExpectOK("+CMEE=2");

    char atCmdString[50];
    sprintf(atCmdString, "+CIPSEND=%d", len);
    while (!(success = sendATExpectResp(atCmdString, ">")) && i++ < 5)
        delay(500);

    if (!success)
        return false;

    DB("ready to transmit");

    delay(DEFAULT_DELAY);

    if (cb_)
        cb_(STATE_TRAFFIC);

    SerialAT.write((const uint8_t *)data, len);
    SerialMon.write((const uint8_t *)data, len);

    SerialAT.write(0x1a);  //^Z to start transmission

    //what for the modem to report something back..
    while (!SerialAT.available())
        ;

    //dump response to serial monitor
    while (SerialAT.available()) {
        SerialMon.write(SerialAT.read());
    }

    if (cb_)
        cb_(STATE_DATA_OK);

    return 1;
}

uint8_t Onomondo::disconnect() {
    uint8_t ok = 0;
    if (TCPConnectionOK_) {
        ok = sendATExpectOK("+CIPCLOSE");
    }
    if (cb_)
        cb_(STATE_NETWRK_OK);

    return ok;
}

uint8_t Onomondo::sleep() {
    return modem_->poweroff();
}

void Onomondo::_setup() {
#ifdef MODEM_RST
    // Keep reset high
    pinMode(MODEM_RST, OUTPUT);
    digitalWrite(MODEM_RST, HIGH);
#endif

    pinMode(MODEM_PWRKEY, OUTPUT);
    pinMode(MODEM_POWER_ON, OUTPUT);

    // Turn on the Modem power first
    digitalWrite(MODEM_POWER_ON, HIGH);

    // Pull down PWRKEY for more than 1 second according to manual requirements
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(100);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, HIGH);

    // Initialize the indicator as an output
    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LED_ON);

    // on board blue LED
    digitalWrite(LED_GPIO, LED_ON);
}

bool Onomondo::sendATExpectOKBase(const char *cmd) {
    //flush input
    while (SerialAT.available()) {
        SerialAT.read();
    }

    const int timeout = 15000;  //ms
    unsigned long startTime = millis();

    //initially send the AT command
    modem_->sendAT(cmd);  // .write(cmd);

    //wait for some response..
    while (SerialAT.available() == 0) {
        if (millis() - startTime > timeout) {
            DB("Timed out waiting for response..");
            return false;
        }
    }

    //incomming data..
    int index = 0;

    while (SerialAT.available() && index < RESPONSE_BUFFER_SIZE - 1) {
        resp_[index] = SerialAT.read();
        index++;
    }
    resp_[index] = '\0';

    Db("Got response: ");
    DB(resp_);

    //check if "OK" is in the string.
    return strstr(resp_, "OK") != NULL;
}

bool Onomondo::sendATOKResp(const char *cmd, const char *resp) {
    if (!sendATExpectOKBase(cmd)) {
        return false;
    }
    //check if resp is in the response buffer...
    return strstr(resp_, resp) != NULL;
}
