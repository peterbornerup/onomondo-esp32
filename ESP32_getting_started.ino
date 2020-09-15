#include <Temperature_LM75_Derived.h>

#include <dummy.h>
#include "Onomondo.h"
#include <ArduinoJson.h>

#include <Ticker.h>

LEDHandler leds;
Ticker ticker;

// modem (and some LED control)
Onomondo *onomondo;

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 30 * 60	  /* Time ESP32 will go to sleep (in seconds) */

#define LED_GPIO 13
#define LED_ON HIGH
#define LED_OFF LOW

#define V_BATT 36
bool debug = true;

void updateLeds()
{
	// callback for the led handler. This will toggle the leds based on the current state.
	leds.periodicUpdate();
}

// Temperature sensor
Generic_LM75_9_to_12Bit_OneShot tempSensor(&Wire, 0x48);

void setup()
{

	leds.changeState(STATE_ON);

	pinMode(LED_GPIO, OUTPUT);

	// the the callback to a periodic timer
	ticker.attach_ms(20, updateLeds);

	//figure out why it was powered up
	esp_sleep_wakeup_cause_t wakeup_reason;
	wakeup_reason = esp_sleep_get_wakeup_cause();

	//input for battery measurement
	pinMode(V_BATT, INPUT);

	//Setup interrupt on Touch Pad 3 (GPIO15) (for wake up)
	touchAttachInterrupt(T9, callbackTouchpin, 40);

	esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
	//esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, 1);
	esp_sleep_enable_touchpad_wakeup();

	//go back to sleep if the battery wwas just connected.
	if (!(wakeup_reason == 1 || wakeup_reason == 2 || wakeup_reason == 3 || wakeup_reason == 4 || wakeup_reason == 5))
		esp_deep_sleep_start();

	//finally initialize the onomondo demo
	// This is a dirty wrapper that handles modem (basic AT commands) and sets the status for the
	// led handler.
	onomondo = new Onomondo(&leds);

	//temperature sensor
	Wire.begin();
	tempSensor.setResolution(Generic_LM75_9_to_12Bit_OneShot::Resolution_12_bits);
	tempSensor.enableShutdownMode();
	tempSensor.startOneShotConversion();
}
// callback for touch input
void callbackTouchpin()
{
	// good for debug. Power off device if pressed during runtime.
	poweroff();
}

void loop()
{
	//connect to the specified port
	bool connected = onomondo->connect("1.2.3.4", 4321);

	//json document to transmit data
	DynamicJsonDocument doc(400);

	//failure to connect... -> go back to sleep
	if (!connected)
	{
		error();
	}

	//send some payload 5 times..
	uint8_t TCPOk;
	for (int i = 0; i < 5; i++)
	{

		int rawAnalog = analogRead(V_BATT);
		doc["battery"] = (float)rawAnalog / 4096.0 * 3.3 * 1.5;

		int16_t sig1 = onomondo->getSignalQuality();
		int16_t sig2 = onomondo->getSignalQuality();

		//quick dirty fix when signal quality returns 99 (error code) (This happens when we call it rapidly)
		doc["signal"] = sig1 < sig2 ? sig1 : sig2;

		while (tempSensor.checkConversionReady())
		{
			// waiiiiiit
			// measurement IS ready long time ago. Just in case we do it like this.
			delay(1);
		}	

		doc["temperature"] = (float)tempSensor.readTemperatureC(); //add the measurement to the document

		//////////////////////////////////////////////
		////HERE GOES WHATEVER SHOULD BE TRANSMITTED//
		//////////////////////////////////////////////

		//serialize the json doc
		char serializedJson[200];
		int size = serializeJson(doc, serializedJson, sizeof(serializedJson));

		//assuming everything is OK we can now transmit
		//easy with Onomondo -> everything is handled on the serverside.

		TCPOk = onomondo->writeTCP(serializedJson, size);

		if (!TCPOk){
			error();
		}
	}

	//disconnect
	if (connected)
		onomondo->disconnect();
	//flash ledsssss (and make some nice effect)
	leds.changeState(STATE_DONE);
	delay(200);
	leds.changeState(STATE_DATA_OK);
	delay(200);
	leds.changeState(STATE_NETWRK_OK);
	delay(200);
	leds.changeState(STATE_ON);

	//power off leds
	//back to sleep+
	//turn off modem
	poweroff();
}

void poweroff()
{
	onomondo->sleep();
	leds.changeState(STATE_POWER_OFF);
	esp_deep_sleep_start();
}

void error()
{
	leds.changeState(STATE_ERROR);
	// add a small delay
	delay(3000);

	poweroff();
}