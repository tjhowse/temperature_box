// For the OLED interface
#include <Wire.h>
#include <ACROBOTIC_SSD1306.h>

// For the thermocouple
#include "max6675.h"

// For the rotary encoder
#include <Encoder.h>

// For the PID loop
#include <PID_v1.h>

// For saving the PID settings.
#include "EEPROMAnything.h"
#include <EEPROM.h>

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

#define GRAPH_PX_X 5
#define GRAPH_PX_Y 5

#define EEPROM_PID_P 0
#define EEPROM_PID_I 8
#define EEPROM_PID_D 16

// Check this often to see whether it's worth saving the settings to EEPROM.
#define EEPROM_SAVE_INTERVAL_MS 10000

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
unsigned long lastEEPROMSave = 0;
unsigned long now = 0;

// Misc
int counter = 0;
int counter1 = 0;
int i, j;

// Framebuffers
// +1 to allow for sprintf to write the null terminator beyond the printable area
char yellowText[YELLOW_CHAR_N_Y][YELLOW_CHAR_N_X+1];
char blueText[BLUE_CHAR_N_Y][BLUE_CHAR_N_X+1];

// https://playground.arduino.cc/Code/PIDLibrary/
double pidInput, pidOutput, pidSetpoint;

#define DEFAULT_PID_P 2
#define DEFAULT_PID_I 5
#define DEFAULT_PID_D 1
double pidP = DEFAULT_PID_P;
double pidI = DEFAULT_PID_I;
double pidD = DEFAULT_PID_D;

double saved_pidP = 0;
double saved_pidI = 0;
double saved_pidD = 0;

PID myPID(&pidInput, &pidOutput, &pidSetpoint, pidP, pidI, pidD, DIRECT);

#define MENU_MODES_N 4
#define MENU_MODE_SETPOINT 0
#define MENU_MODE_TUNE_P 1
#define MENU_MODE_TUNE_I 2
#define MENU_MODE_TUNE_D 3

// Menu management
char menuModes[MENU_MODES_N][BLUE_CHAR_N_X];
unsigned int menuMode = 0;
volatile bool btnA_falling = false;

static unsigned char graph[GRAPH_PX_Y][GRAPH_PX_X];

void savePID(unsigned char slot = 0)
{
    if (slot > 4) return;
    if ((saved_pidP == pidP) &&
        (saved_pidI == pidI) &&
        (saved_pidD == pidD))
    {
        // No save is neccessary. Don't wear out the EEPROM.
        Serial.println("Not necessary to save the PIDs, they haven't changed.");
        return;
    }
    Serial.println("Writing PIDs to EEPROM");

    EEPROM_writeAnything(slot+EEPROM_PID_P, pidP);
    EEPROM_writeAnything(slot+EEPROM_PID_I, pidI);
    EEPROM_writeAnything(slot+EEPROM_PID_D, pidD);
    saved_pidP = pidP;
    saved_pidI = pidI;
    saved_pidD = pidD;
    EEPROM.commit();
}

void loadPID(unsigned char slot = 0)
{
    if (slot > 4) return;
    Serial.println("Loading PIDs from EEPROM.");
    EEPROM_readAnything(slot+EEPROM_PID_P, pidP);
    EEPROM_readAnything(slot+EEPROM_PID_I, pidI);
    EEPROM_readAnything(slot+EEPROM_PID_D, pidD);
    saved_pidP = pidP;
    saved_pidI = pidI;
    saved_pidD = pidD;
}

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

    // // Print the graph
    // if (menuMode == MENU_MODE_SETPOINT)
    // {
    //     oled.setTextXY(7, 0);
    //     oled.drawBitmap(graph[0], GRAPH_PX_X*GRAPH_PX_Y);   // 1024 pixels for logo
    // }
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
    EEPROM.begin(512);
    Serial.begin(9600);
    delay(100);
    Serial.println("Setup done");

    targetTemp = 100;
    lastCurrentTemperatureUpdate = 0;
    myPID.SetMode(AUTOMATIC);
    myPID.SetOutputLimits(0, relayCycle_ms);

    sprintf(menuModes[0], " Adjust target ");
    sprintf(menuModes[MENU_MODE_TUNE_P], "    Tune P     ");
    sprintf(menuModes[MENU_MODE_TUNE_I], "    Tune I     ");
    sprintf(menuModes[MENU_MODE_TUNE_D], "    Tune D     ");

    attachInterrupt(digitalPinToInterrupt(PIN_BTN_A), ISR_btnA, FALLING);
    for (i = 0; i < GRAPH_PX_X; i++)
        for (j = 0; j < GRAPH_PX_Y; j++)
            graph[j][i] = 0xff;

    lastEEPROMSave = millis();
    loadPID();
    if (isnan(pidP) || (pidP == 0))
    {
        // Assume the defaults in EEPROM are bad
        Serial.println("Bad values in EEPROM, using defaults.");
        pidP = DEFAULT_PID_P;
        pidI = DEFAULT_PID_I;
        pidD = DEFAULT_PID_D;
        savePID();
    }
    myPID.SetTunings(pidP, pidI, pidD);
}

void updateSavedSettings()
{
    if ((millis() - lastEEPROMSave) < EEPROM_SAVE_INTERVAL_MS) return;
    lastEEPROMSave = millis();
    Serial.println("Saving settings to eeprom");
    savePID();
}

void updateGraph()
{
    // Propagate the shit
    for (i = 0; i < GRAPH_PX_X-1; i++)
    {
        for (j = 0; j < GRAPH_PX_Y; j++)
        {
            graph[j][i] = graph[j][i+1];
        }
    }
}

void updateCurrentTempDisplay( float temp )
{
    sprintf(yellowText[1], "Current: %6.1f C", temp);
}

void updateTargetTemp()
{
    if (menuMode != MENU_MODE_SETPOINT) return;
    counter = myEnc.read()/2;
    myEnc.write(0);
    if (counter != 0)
    {
        targetTemp += (counter/abs(counter))*pow(counter, 2);
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

void updateMenuDisplay()
{
    sprintf(blueText[0], menuModes[menuMode]);
    if ((menuMode == MENU_MODE_TUNE_P) ||
        (menuMode == MENU_MODE_TUNE_I) ||
        (menuMode == MENU_MODE_TUNE_D))
    {
        counter = myEnc.read()/2;
        myEnc.write(0);

        switch (menuMode)
        {
            case MENU_MODE_TUNE_P:
                if (counter != 0) pidP += (counter/abs(counter))*pow(counter, 2)*0.1;
                sprintf(blueText[2], "  -");
                sprintf(blueText[3], "   ");
                sprintf(blueText[4], "   ");
                break;
            case MENU_MODE_TUNE_I:
                if (counter != 0) pidI += (counter/abs(counter))*pow(counter, 2)*0.1;
                sprintf(blueText[2], "   ");
                sprintf(blueText[3], "  -");
                sprintf(blueText[4], "   ");
                break;
            case MENU_MODE_TUNE_D:
                if (counter != 0) pidD += (counter/abs(counter))*pow(counter, 2)*0.1;
                sprintf(blueText[2], "   ");
                sprintf(blueText[3], "   ");
                sprintf(blueText[4], "  -");
                break;
        }
        myPID.SetTunings(pidP, pidI, pidD);
        sprintf(blueText[2]+4, "P: %.2f    ", pidP);
        sprintf(blueText[3]+4, "I: %.2f    ", pidI);
        sprintf(blueText[4]+4, "D: %.2f    ", pidD);
    } else {
        sprintf(blueText[2], "                ");
        sprintf(blueText[3], "                ");
        sprintf(blueText[4], "                ");
    }
}

void updateMenu()
{
    if (btnA_falling)
    {
        btnA_falling = false;
        menuMode = (menuMode + 1)%MENU_MODES_N;
    }
    updateMenuDisplay();
}

void loop()
{
    updateMenu();
    updateCurrentTemp();
    updateTargetTemp();
    updatePIDLoop();
    updateRelayState();
    updatedutyCycle_msDisplay((dutyCycle_ms/relayCycle_ms)*100);
    // updateGraph();
    updateDisplay();
    updateSavedSettings();
    delay(50);
}