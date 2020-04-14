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
  bluetooth->initialize();
  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  loadcell.set_scale();
  loadcell.tare();
}

//reading holders
int16_t powerReading = 0;           //in W
uint64_t cumulativeRevolutions = 0; //a represents the total number of times a crank rotates

uint64_t lastCET = 0;        
/*lastCrankEvent: The 'crank event time' is a free-running-count of 1/1024 second units and it 
represents the time when the crank revolution was detected by the crank rotation sensor. Since
 several crank events can occur between transmissions, only the Last Crank Event Time value is
  transmitted. This value is used in combination with the Cumulative Crank Revolutions value to
   enable the Client to calculate cadence. The Last Crank Event Time value rolls over every 64 seconds.
*/

/*Cadence = (Difference in two successive Cumulative Crank Revolution values) /
(Difference in two successive Last Crank Event Time values)
*/


bool deviceConnected = false;
bool oldDeviceConnected = false;
uint16_t loopDelay = 1024U; //to help in artificial rpm generation

void loop()
{
  // notify changed value
  if (deviceConnected)
  {

    //get data  (fake for now)
    powerReading += 3;
    cumulativeRevolutions+=2; //hard set for 120rpm
    lastCET+=loopDelay; //hard set for 120rpm

    //BLE Transmit
    bluetooth->sendValueThroughPower(loadcell.get_units(10)); //sends the raw uncalibrated HX711 value to read it on nRF Connect and make calibration spreadsheets

    //bluetooth->sendPower(powerReading); //the release-ready power send function, for an already calibrated and working device.
    bluetooth->sendCSC(lastCET, cumulativeRevolutions);

    delay(loopDelay); // the minimum is 3ms according to official docs
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected)
  {
    bluetooth->startBroadcast();
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}