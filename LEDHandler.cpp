#include "LEDHandler.h"
#include "Arduino.h"

#define LED1 19
#define LED2 18
#define LED3 25 //5
#define LED4 14 //4
#define LED5 12
#define LED6 2

LEDHandler::LEDHandler()
{
	pinMode(LED1, OUTPUT);
	pinMode(LED2, OUTPUT);
	pinMode(LED3, OUTPUT);
	pinMode(LED4, OUTPUT);
	pinMode(LED5, OUTPUT);
	pinMode(LED6, OUTPUT);

	for (int i = 0; i < 6; i++)
	{
		leds[i] = 0;
	}
}

LEDHandler::~LEDHandler()
{
}

void LEDHandler::changeState(const modemStates &newState)
{
	state_ = newState;
	ledUpdateCallback_();
}

//set leds base on state. Called frequently by ticker.
void LEDHandler::ledUpdateCallback_()
{

	//toggle flash state
	if (millis() - lastUpdate_ > blinkWaitMs_ / 2)
	{
		rapidFlash_ = rapidFlash_ ? 0 : 1;

		if (rapidFlash_)
		{
			flashState_ = flashState_ ? 0 : 1;
		}

		lastUpdate_ = millis();
	}

	//update based on state
	switch (state_)
	{
	case STATE_POWER_OFF:
		for (size_t i = 0; i < 6; i++)
		{ //power off everything
			leds[i] = 0;
		}
		break;
	case STATE_ON:
		for (size_t i = 0; i < 6; i++)
		{ //power off everything
			leds[i] = 0;
		}
		leds[0] = 1;
		break;
	case STATE_NETWORK:
		leds[0] = 1;
		leds[1] = flashState_;
		leds[2] = 0;
		leds[3] = 0;
		leds[4] = 0;
		leds[5] = 0;
		break;
	case STATE_NETWRK_OK:
		leds[0] = 1;
		leds[1] = 1;
		leds[2] = 0;
		leds[3] = 0;
		leds[4] = 0;
		leds[5] = 0;
		break;
	case STATE_DATA:
		leds[0] = 1;
		leds[1] = 1;
		leds[2] = flashState_;
		leds[3] = 0;
		leds[4] = 0;
		leds[5] = 0;
		break;
	case STATE_DATA_OK:
		leds[0] = 1;
		leds[1] = 1;
		leds[2] = 1;
		leds[3] = 0;
		leds[4] = 0;
		leds[5] = 0;
		break;
	case STATE_TRAFFIC:
		leds[0] = 1;
		leds[1] = 1;
		leds[2] = 1;
		leds[3] = rapidFlash_;
		leds[4] = 0;
		leds[5] = 0;
		break;
	case STATE_DONE:
		leds[0] = 1;
		leds[1] = 1;
		leds[2] = 1;
		leds[3] = 0;
		leds[4] = 1;
		leds[5] = 0;
		break;
	case STATE_ERROR:
		leds[5] = rapidFlash_;
		break;

	default:
		break;
	}

	digitalWrite(LED4, leds[0]);
	digitalWrite(LED5, leds[1]);
	digitalWrite(LED3, leds[2]);
	digitalWrite(LED6, leds[3]);
	digitalWrite(LED2, leds[4]);
	digitalWrite(LED1, leds[5]);
}
