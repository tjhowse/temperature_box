// For the OLED interface
#include <Wire.h>
#include <ACROBOTIC_SSD1306.h>

// For the thermocouple
#include "max6675.h"

// For the rotary encoder
#include <Encoder.h>

// For the PID loop
#include <PID_v1.h>

// Constants
#define CURRENT_TEMPERATURE_UPDATE_INTERVAL_MS 500

// Pin assignments
#define PIN_THERMO_D0 12 // D6
#define PIN_THERMO_CS 16 // D0
#define PIN_TERMO_CLK 15 // D8
#define PIN_ENC_A 14 // D5
#define PIN_ENC_B 13 // D7
#define PIN_BTN_A 0 // D3
#define PIN_RELAY 2 // D4 LED

// Screen characteristics
#define PX_X 128
#define PX_Y 64
#define YELLOW_CHAR_N_X 25
#define YELLOW_CHAR_N_Y 2
#define BLUE_CHAR_N_X 16
#define BLUE_CHAR_N_Y 6

MAX6675 thermocouple(PIN_TERMO_CLK, PIN_THERMO_CS, PIN_THERMO_D0);
Encoder myEnc(PIN_ENC_A, PIN_ENC_B);

unsigned int relayCycle_ms = 3000;
float currentTemp;
float targetTemp;
float dutyCycle_ms; // The amount of time the relay is on during a cycle. 0 to relayCycle_ms
bool relayState; // 0 - Open circuit, 1 - Closed circuit

// Times
unsigned long lastCurrentTemperatureUpdate = 0;
unsigned long lastRelayCycleTime = 0;
unsigned long now = 0;

// Misc
int counter = 0;
int counter1 = 0;
int i, j;

// Framebuffers
char yellowText[YELLOW_CHAR_N_Y][YELLOW_CHAR_N_X];
char blueText[BLUE_CHAR_N_Y][BLUE_CHAR_N_X];

// https://playground.arduino.cc/Code/PIDLibrary/
double pidInput, pidOutput, pidSetpoint;

double pidP = 2;
double pidI = 5;
double pidD = 1;

PID myPID(&pidInput, &pidOutput, &pidSetpoint, pidP, pidI, pidD, DIRECT);

// Menu management
char menuModes[4][BLUE_CHAR_N_X];
unsigned int menuMode = 0;
volatile bool btnA_falling = false;

void updateDisplay()
{
    // Update the yellow rows
    // oled.clearDisplay();
    oled.setFont(font5x7);

    for (i = 0; i < YELLOW_CHAR_N_Y; i++)
    {
        for (j = 0; j < YELLOW_CHAR_N_X; j++)
            if (yellowText[i][j] == '\0')
                yellowText[i][j] = ' ';
        yellowText[i][YELLOW_CHAR_N_X-1] = '\0';
        oled.setTextXY(i, 0);
        oled.putString(yellowText[i]);
    }

    // Update the blue rows
    oled.setFont(font8x8);
    for (i = 0; i < BLUE_CHAR_N_Y; i++)
    {
        for (j = 0; j < BLUE_CHAR_N_X; j++)
            if (blueText[i][j] == '\0')
                blueText[i][j] = ' ';
        blueText[i][BLUE_CHAR_N_X-1] = '\0';
        oled.setTextXY(i+2, 0);
        oled.putString(blueText[i]);
    }
}

void ISR_btnA() {
  btnA_falling = true;
}

void setup()
{
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_ENC_A, OUTPUT);
    pinMode(PIN_ENC_B, OUTPUT);
    pinMode(PIN_RELAY, OUTPUT);
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
    myPID.SetMode(AUTOMATIC);
    myPID.SetOutputLimits(0, relayCycle_ms);

    sprintf(menuModes[0], "Adjust target");
    sprintf(menuModes[1], "Borpus");
    sprintf(menuModes[2], "Gorpus");

    attachInterrupt(digitalPinToInterrupt(PIN_BTN_A), ISR_btnA, FALLING)
}

void updateCurrentTempDisplay( float temp )
{
    sprintf(yellowText[1], "Current: %6.1f C", temp);
}

void updateTargetTemp()
{
    counter = myEnc.read()/2;
    if (counter != 0)
    {
        targetTemp += (counter/abs(counter))*pow(counter, 2);
        myEnc.write(0);
    }
    updateTargetTempDisplay();
}

void updateTargetTempDisplay()
{
    sprintf(yellowText[0], "Target:  %6.1f C", targetTemp);
}

void updatedutyCycle_msDisplay(int dutyPercent)
{
    sprintf(yellowText[0] + YELLOW_CHAR_N_X-5, "Duty");
    sprintf(yellowText[1] + YELLOW_CHAR_N_X-5, "%3d%%", dutyPercent);
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

void updatePIDLoop()
{
    pidInput = currentTemp;
    pidSetpoint = targetTemp;
    myPID.Compute();
    dutyCycle_ms = pidOutput;
}

void updateRelayState()
{
    now = millis();
    if ((now-lastRelayCycleTime) >= relayCycle_ms)
    {
        lastRelayCycleTime = now;
        // This is correcter, but dangerouser
        // lastRelayCycleTime += relayCycle_ms;
    }
    // If we're in the first part of the cycle, I.E. between 0 and dutyCycle_ms after lastRelayCycleTime,
    // the relay should be on. Otherwise: off.
    relayState = ((now-lastRelayCycleTime) < dutyCycle_ms);
    digitalWrite(PIN_RELAY, !relayState);
}

void loop()
{
    updateCurrentTemp();
    updateTargetTemp();
    updatePIDLoop();
    updateRelayState();
    updatedutyCycle_msDisplay((dutyCycle_ms/relayCycle_ms)*100);
    updateDisplay();
    delay(50);
}