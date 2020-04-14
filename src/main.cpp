#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>
#include "globals.h"
#include "HX711.h"
#include "MPU6050.h"

#include "BLEPowerCSC.cpp" //TODO can this be swapped out for an .h?

BLEPowerCSC *bluetooth = new BLEPowerCSC();
HX711 loadcell;

// 1. HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 3;

//dviceConencted and oldDeviceConnected are defined in blepowercsc

void setup()
{
  Serial.begin(115200);
  bluetooth->initialize();
  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  loadcell.set_scale();
  loadcell.tare();
}

//reading holders
int16_t powerReading = 0;           //in W
uint64_t cumulativeRevolutions = 0; //a number
uint64_t lastCET = 1024;            //fixed for 60rpm // in 1/1024 s intervals

bool deviceConnected = false;
bool oldDeviceConnected = false;

void loop()
{
  // notify changed value
  if (deviceConnected)
  {

    //get data  (fake for now)
    powerReading += 3;
    cumulativeRevolutions++;
    lastCET = (lastCET == 1024) ? 0 : 1024;

    //BLE Transmit
    bluetooth->sendValueThroughPower(loadcell.get_units(10)); //sends the raw uncalibrated HX711 value to read it on nRF Connect and make calibration spreadsheets

    //bluetooth->sendPower(powerReading); //the release-ready power send function, for an already calibrated and working device.
    bluetooth->sendCSC(lastCET, cumulativeRevolutions);

    // Serial.print("Power: ");
    // Serial.print(powerReading);
    // Serial.println("W");
    // Serial.print("Cadence data: ");
    // Serial.print((long)cumulativeRevolutions);
    // Serial.print(" revs and LCET:");
    // Serial.println((long)lastCET);

    delay(1000); // the minimum is 3ms according to official docs
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected)
  {
    bluetooth->startBroadcast();
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}