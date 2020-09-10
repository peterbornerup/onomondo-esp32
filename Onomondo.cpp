#include "Onomondo.h"

#define DB SerialMon.println
#define Db SerialMon.print

#define DEFAULT_DELAY 300

Onomondo::Onomondo(LEDHandler *leds) : leds_(leds)
{

	// Set console baud rate
	SerialMon.begin(115200);

#ifdef DUMP_AT_COMMANDS //yeee. 
	debugger_ = new StreamDebugger(SerialAT, SerialMon);
	modem_ = new TinyGsm(*debugger_);
#else
	modem_ = new TinyGsm(SerialAT);
#endif // DUMP_AT_COMMANDS

	// PMU settings (specific to the ESP32 board used)
	Wire.beginTransmission(IP5306_ADDR);
	int bytes = Wire.write(IP5306_REG_SYS_CTL0);

	bytes = Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
	Wire.endTransmission() == 0;

	//init GPIO and start modem
	_setup();

	delay(10);

	// Set GSM module baud rate and UART pins
	SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

	delay(10);
}

Onomondo::~Onomondo()
{
}

uint8_t Onomondo::connect(char *server, int port)
{

	bool success = false;

	success = modem_->init();

	//initialize the modem
	if (!success)
	{
		success = modem_->restart();

		if (!success)
		{
			return false;
		}
	}

	//we now register on the network
	if (leds_)
		leds_->changeState(STATE_NETWORK);

	if (!modem_->waitForNetwork(60000L))
	{
		Db("Failed to register to the network");
		return 0;
	}

	if (!modem_->isNetworkConnected())
	{
		DB("not connected to network...");
		return 0;
	}

	//network OK
	if (leds_)
		leds_->changeState(STATE_NETWRK_OK);

	delay(DEFAULT_DELAY);

	if (leds_)
		leds_->changeState(STATE_DATA);


	//set apn
	int n = 0;
	while (!(success = sendATExpectOK("+CSTT=\"onomondo\"")) && n++ < 6)
	{
		DB("Retrying... cstt");
		delay(1000);
	}
	if (!success){
		DB("failed to set APN: onomondo (+CSTT)")
		return 0;
	}

	delay(DEFAULT_DELAY);

	//status
	//WE MUST WAIT FOR A "1" RESPONSE
	while (!(success = sendATOKResp("+CGATT?", "1")) && n++ < 10)
	{
		DB("CGATT>>>");
		delay(500);
	}
	if (!success){
		Db("Failed to attach to packet domain service (+CGATT)");
		return 0;
	}

	delay(DEFAULT_DELAY);

	//bring up GPRS
	while (!(success = sendATExpectOK("+CIICR")) && n++ < 10)
	{
		//DB("Retrying... CIICR");
		delay(1000);
	}
	if (!success){
		DB("Failed to bring up GPRS. Maybe the data is blocked by onomondo? (+CIICR)");
		return 0;
	}

	delay(DEFAULT_DELAY);

	//ip adress //must be read for some reason. See SIM800 datasheet
	modem_->sendAT("+CIFSR");
	//wait for response
	while (!SerialAT.available());
	while (SerialAT.available())
	{
		SerialMon.write(SerialAT.read());
	}

	char buffer[50];

	//initialize TCP Stack with server and port
	sprintf(buffer, "+CIPSTART=\"TCP\",\"%s\",\"%u\"", server, port);
	//Db("CMD: "); DB(buffer);

	delay(DEFAULT_DELAY);
	//connect TCP //wait for OK
	while (!(success = sendATExpectOK(buffer)) && n++ < 4)
	{
		DB("Retrying... tcp connection stuff");
		delay(1000);
	}
	if (!success){
		DB("Modem failed to initialize TCP stack..");
		return 0;
	}

	TCPConnectionOK_ = true;

	if (leds_)
		leds_->changeState(STATE_DATA_OK);

	return 1;
}

uint8_t Onomondo::writeTCP(const char *data, const int &len)
{

	if (!TCPConnectionOK_)
		return 0;

	bool success = false;
	int i = 0;

	while (!(success = sendATExpectResp("+CIPSEND", ">")) && i++ < 5)
		delay(500);

	if (!success)
		return false;

	DB("ready to transmit");

	delay(DEFAULT_DELAY);

	if (leds_)
		leds_->changeState(STATE_TRAFFIC);

	SerialAT.write((const uint8_t *)data, len);
	SerialMon.write((const uint8_t *)data, len);
	//SerialAT.write(data,len);
	SerialAT.write(0x1a); //^Z to start transmission

	//what for the modem to report something back..
	while (!SerialAT.available())
		;

	//dump response to serial monitor
	while (SerialAT.available())
	{
		SerialMon.write(SerialAT.read());
	}

	if (leds_)
		leds_->changeState(STATE_DATA_OK);

	return 1;
}

uint8_t Onomondo::disconnect()
{
	uint8_t ok = 0;
	if (TCPConnectionOK_)
	{
		ok = sendATExpectOK("+CIPCLOSE");
	}

	if (leds_)
		leds_->changeState(STATE_NETWRK_OK);

	return ok;
}

uint8_t Onomondo::sleep()
{
	return modem_->poweroff();
}

void Onomondo::_setup()
{
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

	//modem_->sendAT("+CFUN=1");
	//delay(1000);

	// on board blue LED
	digitalWrite(LED_GPIO, LED_ON);
}

bool Onomondo::sendATExpectOKBase(const char *cmd)
{

	//flush input
	while (SerialAT.available())
	{
		SerialAT.read();
	}

	const int timeout = 15000; //ms
	unsigned long startTime = millis();

	//initially send the AT command
	modem_->sendAT(cmd); // .write(cmd);

	//wait for some response..
	while (SerialAT.available() == 0)
	{
		if (millis() - startTime > timeout)
		{
			DB("Timed out waiting for response..");
			return false;
		}
	}

	//incomming data..
	int index = 0;

	while (SerialAT.available() && index < RESPONSE_BUFFER_SIZE - 1)
	{
		resp_[index] = SerialAT.read();

		//Db(resp_[index]);

		index++;
	}
	resp_[index] = '\0';

	Db("Got response: ");
	DB(resp_);

	//check if "OK" is in the string.
	bool okExist = strstr(resp_, "OK") != NULL;

	return okExist;
}

bool Onomondo::sendATOKResp(const char *cmd, const char *resp)
{

	if (!sendATExpectOKBase(cmd))
	{
		return false;
	}

	//check if resp is in the response buffer...
	bool ok = strstr(resp_, resp) != NULL;

	return ok;
}