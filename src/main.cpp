#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>
#include "globals.h"

#include "BLEPowerCSC.cpp" //TODO can this be swapped out for an .h?

BLEPowerCSC *bluetooth = new BLEPowerCSC();

//dviceConencted and oldDeviceConnected are defined in blepowercsc

void setup()
{
  Serial.begin(115200);
  bluetooth->initialize();
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

    //send it
    bluetooth->sendPower(powerReading);
    bluetooth->sendCSC(lastCET, cumulativeRevolutions);
    Serial.print("Power: ");
    Serial.print(powerReading);
    Serial.println("W");
    Serial.print("Cadence data: ");
    Serial.print((long)cumulativeRevolutions);
    Serial.print(" revs and LCET:");
    Serial.println((long)lastCET);

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