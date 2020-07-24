/*
  06/01/2016
  Author: Makerbro
  Platforms: ESP8266
  Language: C++
  File: HelloOLED.ino
  ------------------------------------------------------------------------
  Description:
  Demo for OLED display showcasing writing text to the screen.
  ------------------------------------------------------------------------
  Please consider buying products from ACROBOTIC to help fund future
  Open-Source projects like this! We'll always put our best effort in every
  project, and release all our design files and code for you to use.
  https://acrobotic.com/
  ------------------------------------------------------------------------
  License:
  Released under the MIT license. Please check LICENSE.txt for more
  information.  All text above must be included in any redistribution.
*/
#include <Wire.h>
#include <ACROBOTIC_SSD1306.h>
#include "max6675.h"
#include <Encoder.h>

#define CURRENT_TEMPERATURE_UPDATE_INTERVAL_MS 500
#define PIN_THERMO_D0 12 // D6
#define PIN_THERMO_CS 16 // D0
#define PIN_TERMO_CLK 15 // D8
#define PIN_ENC_A 14 // D5
#define PIN_ENC_B 13 // D7
#define PIN_BTN_A 0 // D3

#define OLED_DISPLAY_MS 5

#define PX_X 128
#define PX_Y 64
#define YELLOW_CHAR_N_X 25
#define YELLOW_CHAR_N_Y 2
#define BLUE_CHAR_N_X 16
#define BLUE_CHAR_N_Y 6

MAX6675 thermocouple(PIN_TERMO_CLK, PIN_THERMO_CS, PIN_THERMO_D0);
Encoder myEnc(PIN_ENC_A, PIN_ENC_B);

float currentTemp;
float targetTemp;
char currentTempDisplayRow[20];
char targetTempDisplayRow[20];
char debugMessageString[20];

unsigned long lastCurrentTemperatureUpdate = 0;

int counter = 0;
int counter1 = 0;
char yellowText[YELLOW_CHAR_N_Y][YELLOW_CHAR_N_X];
char blueText[BLUE_CHAR_N_Y][BLUE_CHAR_N_X];
int i, j;

void updateDisplay()
{
    // Update the yellow rows
    // oled.clearDisplay();
    oled.setFont(font5x7);

    for (i = 0; i < YELLOW_CHAR_N_Y; i++)
    {
        oled.setTextXY(i, 0);
        oled.putString(yellowText[i]);
    }

    // Update the blue rows
    oled.setFont(font8x8);
    for (i = 0; i < BLUE_CHAR_N_Y; i++)
    {
        oled.setTextXY(i+2, 0);
        oled.putString(blueText[i]);
    }

}

void setup()
{
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_ENC_A, OUTPUT);
    pinMode(PIN_ENC_B, OUTPUT);
    digitalWrite(PIN_ENC_A, HIGH);
    digitalWrite(PIN_ENC_B, HIGH);

    Wire.begin();
    oled.init();
    oled.setFont(font8x8);
    oled.clearDisplay();
    targetTemp = 100;
    lastCurrentTemperatureUpdate = 0;
    // Serial.begin(9600);
    // delay(100);
    // Serial.println("Setup done");
}

void updateCurrentTempDisplay( float temp )
{
    sprintf(yellowText[1], "Current: %6.2f C", temp);
}

void updateTargetTempDisplay( float temp )
{
    sprintf(yellowText[0], "Target: %6.2f C", temp);
}

void updateDutyCycleDisplay(int dutyCycle)
{
    sprintf(&yellowText[0][YELLOW_CHAR_N_X-5], "Duty");
    sprintf(&yellowText[1][YELLOW_CHAR_N_X-5], "%d%", dutyCycle);
}


void debugMessage(int row, char* message)
{
    sprintf(blueText[BLUE_CHAR_N_Y-1], message);
}

void updateCurrentTemp()
{
    if ((millis() - lastCurrentTemperatureUpdate) < CURRENT_TEMPERATURE_UPDATE_INTERVAL_MS) return;
    lastCurrentTemperatureUpdate = millis();

    currentTemp = thermocouple.readCelsius();
    // Serial.println(currentTemp);
    updateCurrentTempDisplay(currentTemp);
}

void loop()
{
    updateCurrentTemp();

    targetTemp += myEnc.read()/2;
    myEnc.write(0);

    updateTargetTempDisplay(targetTemp);
    // updateDutyCycleDisplay(100);
    // sprintf(yellowText[0], "Yellow");
    // sprintf(blueText[0], "Blue");
    updateDisplay();
    delay(50);
}