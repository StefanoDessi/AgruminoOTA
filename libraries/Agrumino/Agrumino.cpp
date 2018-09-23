/*
  Agrumino.cpp - Library for Agrumino board - Version 0.2 for Board R3
  Created by giuseppe.broccia@lifely.cc on October 2017.
  Updated on March 2018

  For details @see Agrumino.h
*/

#include "Agrumino.h"
#include <Wire.h>
#include "libraries/MCP9800/MCP9800.cpp"
#include "libraries/PCA9536_FIX/PCA9536_FIX.cpp" // PCA9536.h lib has been modified (REG_CONFIG renamed to REG_CONFIG_PCA) to avoid name clashing with mcp9800.h
#include "libraries/MCP3221/MCP3221.cpp"

// PINOUT Agrumino        Implemented
#define PIN_SDA          2 // [X] BOOT: Must be HIGH at boot
#define PIN_SCL         14 // [X] 
#define PIN_PUMP        12 // [X] 
#define PIN_BTN_S1       4 // [X] Same as Internal WT8266 LED
#define PIN_USB_DETECT   5 // [X] 
#define PIN_MOSFET      15 // [X] BOOT: Must be LOW at boot
#define PIN_BATT_STAT   13 // [X] 
#define PIN_LEVEL        0 // [ ] BOOT: HIGH for Running and LOW for Program

// Addresses I2C sensors       Implemented
#define I2C_ADDR_SOIL      0x4D // [X] TODO: Save air and water values to EEPROM
#define I2C_ADDR_LUX       0x44 // [X] 
#define I2C_ADDR_TEMP      0x48 // [X] 
#define I2C_ADDR_GPIO_EXP  0x41 // [-] BOTTOM LED OK, TODO: GPIO 2-3-4 

////////////
// CONFIG //
////////////

// GPIO Expander
#define IO_PCA9536_LED                IO0 // Bottom Green led is connected to GPIO0 of the PCA9536 GPIO expander
// Soil values from a brand new device with the sensor in the air / immersed by 3/4 in water
#define DEFAULT_SOIL_RAW_AIR         3400 // Lifely R3 Capacitive Flat
#define DEFAULT_SOIL_RAW_WATER       1900 // Lifely R3 Capacitive Flat
// #define DEFAULT_SOIL_RAW_AIR         3400 // Lifely R3 Capacitive with holes
// #define DEFAULT_SOIL_RAW_WATER       1650 // Lifely R3 Capacitive with holes
// Battery
#define BATTERY_MILLIVOLT_LEVEL_0    3500 // Voltage of a fully discharged battery (Safe value, cut-off of a LIR2450 is 2.75 V)
#define BATTERY_MILLIVOLT_LEVEL_100  4200 // Voltage of a fully charged battery
#define BATTERY_VOLT_DIVIDER_Z1      1800 // Value of the Z1(R25) resistor in the Voltage divider used for read the batt voltage.
#define BATTERY_VOLT_DIVIDER_Z2       424 // 470 (Original) // Value of the Z2(R26) resistor. Adjusted considering the ADC internal resistance.
#define BATTERY_VOLT_SAMPLES           20 // Number of reading needed to calculate the battery voltage

///////////////
// Variables //
///////////////

MCP9800 mcpTempSensor;
PCA9536 pcaGpioExpander;
MCP3221 mcpSoilSensor(I2C_ADDR_SOIL);
unsigned int _soilRawAir;
unsigned int _soilRawWater;

/////////////////
// Constructor //
/////////////////

Agrumino::Agrumino() {
}

void Agrumino::setup() {
  setupGpioModes();
  printLogo();
  // turnBoardOn(); // Decomment to have the board On by Default
}

void Agrumino::deepSleepSec(unsigned int sec) {
  if (sec > 4294) {
    // ESP.deepSleep argument is an unsigned int, so the max allowed walue is 0xffffffff (4.294.967.295).
    sec = 4294;
    Serial.println("Warning: deepSleep can be max 4294 sec (~71 min). Value has been constrained!");
  }

  Serial.print("\nGoing to deepSleep for ");
  Serial.print(sec);
  Serial.println(" seconds... (ー。ー) zzz\n");
  ESP.deepSleep(sec * 1000000); // microseconds
}

/////////////////////////
// Public methods GPIO //
/////////////////////////

boolean Agrumino::isAttachedToUSB() {
  return digitalRead(PIN_USB_DETECT) == HIGH;
}

boolean Agrumino::isBatteryCharging() {
  return digitalRead(PIN_BATT_STAT) == LOW;
}

boolean Agrumino::isButtonPressed() {
  return digitalRead(PIN_BTN_S1) == LOW;
}

boolean Agrumino::isBoardOn() {
  return digitalRead(PIN_MOSFET) == HIGH;
}

void Agrumino::turnBoardOn() {
  if (!isBoardOn()) {
    digitalWrite(PIN_MOSFET, HIGH);
    delay(5); // Ensure that the ICs are booted up properly
    initBoard();
    checkBattery();
  }
}
void Agrumino::turnBoardOff() {
  digitalWrite(PIN_MOSFET, LOW);
}

void Agrumino::turnWateringOn() {
  digitalWrite(PIN_PUMP, HIGH);
}

void Agrumino::turnWateringOff() {
  digitalWrite(PIN_PUMP, LOW);
}

////////////////////////
// Public methods I2C //
////////////////////////

float Agrumino::readTempC() {
  return mcpTempSensor.readCelsiusf();
}

float Agrumino::readTempF() {
  return mcpTempSensor.readFahrenheitf();
}

void Agrumino::turnLedOn() {
  pcaGpioExpander.setState(IO_PCA9536_LED, IO_HIGH);
}

void Agrumino::turnLedOff() {
  pcaGpioExpander.setState(IO_PCA9536_LED, IO_LOW);
}

boolean Agrumino::isLedOn() {
  return pcaGpioExpander.getState(IO_PCA9536_LED) == 1;
}

void Agrumino::toggleLed() {
  pcaGpioExpander.toggleState(IO_PCA9536_LED);
}

void Agrumino::calibrateSoilWater() {
  _soilRawWater = readSoilRaw();
}

void Agrumino::calibrateSoilAir() {
  _soilRawAir = readSoilRaw();
}

void Agrumino::calibrateSoilWater(unsigned int rawValue) {
  _soilRawWater = rawValue;
}

void Agrumino::calibrateSoilAir(unsigned int rawValue) {
  _soilRawAir = rawValue;
}

unsigned int Agrumino::readSoil() {
  unsigned int soilRaw = readSoilRaw();
  soilRaw = constrain(soilRaw, _soilRawWater, _soilRawAir);
  return map(soilRaw, _soilRawAir, _soilRawWater, 0, 100);
}

float Agrumino::readLux() {
  // Logic for Light-to-Digital Output Sensor ISL29003
  Wire.beginTransmission(I2C_ADDR_LUX);
  Wire.write(0x02); // Data registers are 0x02->LSB and 0x03->MSB
  Wire.endTransmission();
  Wire.requestFrom(I2C_ADDR_LUX, 2); // Request 2 bytes of data
  unsigned int data;
  if (Wire.available() == 2) {
    byte lsb = Wire.read();
    byte  msb = Wire.read();
    data = (msb << 8) | lsb;
  } else {
    Serial.println("readLux Error!");
    return 0;
  }
  // Convert the data from the ADC to lux
  // 0-64000 is the selected range of the ALS (Lux)
  // 0-65536 is the selected range of the ADC (16 bit)
  return (64000.0 * (float) data) / 65536.0;
}

float Agrumino::readBatteryVoltage() {
  float voltSum = 0.0;
  for (int i = 0; i < BATTERY_VOLT_SAMPLES; ++i) {
    voltSum += readBatteryVoltageSingleShot();
  }
  float volt = voltSum / BATTERY_VOLT_SAMPLES;
  // Serial.println("readBatteryVoltage: " + String(volt) + " V");
  return volt;
}

unsigned int Agrumino::readBatteryLevel() {
  float voltage = readBatteryVoltage();
  unsigned int milliVolt = (int) (voltage * 1000.0);
  milliVolt = constrain(milliVolt, BATTERY_MILLIVOLT_LEVEL_0, BATTERY_MILLIVOLT_LEVEL_100);
  return map(milliVolt, BATTERY_MILLIVOLT_LEVEL_0, BATTERY_MILLIVOLT_LEVEL_100, 0, 100);
}

/////////////////////
// Private methods //
/////////////////////

void Agrumino::initGpioExpander() {
  Serial.print("initGpioExpander → ");
  byte result = pcaGpioExpander.ping();;
  if (result == 0) {
    pcaGpioExpander.reset();
    pcaGpioExpander.setMode(IO_PCA9536_LED, IO_OUTPUT); // Back green LED
    pcaGpioExpander.setState(IO_PCA9536_LED, IO_LOW);
    Serial.println("OK");
  } else {
    Serial.println("FAIL!");
  }
}

void Agrumino::initTempSensor() {
  Serial.print("initTempSensor   → ");
  boolean success = mcpTempSensor.init(true);
  if (success) {
    mcpTempSensor.setResolution(MCP_ADC_RES_11); // 11bit (0.125c)
    mcpTempSensor.setOneShot(true);
    Serial.println("OK");
  } else {
    Serial.println("FAIL!");
  }
}

void Agrumino::initSoilSensor() {
  Serial.print("initSoilSensor   → ");
  byte response = mcpSoilSensor.ping();;
  if (response == 0) {
    mcpSoilSensor.reset();
    mcpSoilSensor.setSmoothing(EMAVG);
    // mcpSoilSensor.setVref(3300); This will make the reading of the MCP3221 voltage accurate. Currently is not needed because we need just a range
    _soilRawAir = DEFAULT_SOIL_RAW_AIR;
    _soilRawWater = DEFAULT_SOIL_RAW_WATER;
    Serial.println("OK");
  } else {
    Serial.println("FAIL!");
  }
}

void Agrumino::initLuxSensor() {
  // Logic for Light-to-Digital Output Sensor ISL29003
  Serial.print("initLuxSensor    → ");
  Wire.beginTransmission(I2C_ADDR_LUX);
  byte result = Wire.endTransmission();
  if (result == 0) {
    Wire.beginTransmission(I2C_ADDR_LUX);
    Wire.write(0x00); // Select "Command-I" register
    Wire.write(0xA0); // Select "ALS continuously" mode
    Wire.endTransmission();
    Wire.beginTransmission(I2C_ADDR_LUX);
    Wire.write(0x01); // Select "Command-II" register
    Wire.write(0x03); // Set range = 64000 lux, ADC 16 bit
    Wire.endTransmission();
    Serial.println("OK");
  } else {
    Serial.println("FAIL!");
  }
}

unsigned int Agrumino::readSoilRaw() {
  return mcpSoilSensor.getVoltage();
}

float Agrumino::readBatteryVoltageSingleShot() {
  float z1 = (float) BATTERY_VOLT_DIVIDER_Z1;
  float z2 = (float) BATTERY_VOLT_DIVIDER_Z2;
  float vread = (float) analogRead(A0) / 1024.0; // RAW Value from the ADC, Range 0-1V
  float volt = ((z1 + z2) / z2) * vread;
  return volt;
}

// Return true if the battery is ok
// Return false and put the ESP to sleep if not
boolean Agrumino::checkBattery() {
  if (readBatteryLevel() > 0) {
    return true;
  } else {
    Serial.print("\nturnBoardOn Fail! Battery is too low!!!\n");
    deepSleepSec(3600); // Sleep for 1 hour
    return false;
  }
}

void Agrumino::initWire() {
  Wire.begin(PIN_SDA, PIN_SCL);
}

void Agrumino::initBoard() {
  initWire();
  initLuxSensor();  // Boot time depends on the selected ADC resolution (16bit first reading after ~90ms)
  initSoilSensor(); // First reading after ~30ms
  initTempSensor(); // First reading after ~?ms
  initGpioExpander(); // First operation after ~?ms
  delay(90); // Ensure that the ICs are init properly
}

void Agrumino::setupGpioModes() {
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_BTN_S1, INPUT_PULLUP);
  pinMode(PIN_USB_DETECT, INPUT);
  pinMode(PIN_MOSFET, OUTPUT);
  pinMode(PIN_BATT_STAT, INPUT_PULLUP);
}

void Agrumino::printLogo() {
  Serial.println("\n\n\n");
  Serial.println(" ____________________________________________");
  Serial.println("/\\      _                       _            \\");
  Serial.println("\\_|    /_\\  __ _ _ _ _  _ _ __ (_)_ _  ___   |");
  Serial.println("  |   / _ \\/ _` | '_| || | '  \\| | ' \\/ _ \\  |");
  Serial.println("  |  /_/ \\_\\__, |_|  \\_,_|_|_|_|_|_||_\\___/  |");
  Serial.println("  |        |___/                  By Lifely  |");
  Serial.println("  |  ________________________________________|_");
  Serial.println("  \\_/_________________________________________/");
  Serial.println("");
}
