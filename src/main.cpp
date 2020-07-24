#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>
#include "globals.h"
#include "HX711.h"
#include "Wire.h"
#include "MPU6050.h"

#include "BLEBridge.cpp" //TODO can this be swapped out for an .h?

#define LED_PIN 2
#define minAccelVal -16383
#define maxAccelVal 16384

const double crankLength = 0.1725; //in meters
const int LOADCELL_DOUT_PIN = 22;
const int LOADCELL_SCK_PIN = 23;
const double LOADCELL_SCALE_VALUE = 1650;

const int ACCEL_SCL_PIN = 19;
const int ACCEL_SDA_PIN = 21;

BLEBridge *bluetooth = new BLEBridge();
HX711 loadcell;
MPU6050 accel; //I2C address 0x53

/*
90 degrees = crank pointing up
180 degrees = crank pointing forwards (max torque)
*/
uint16_t getCrankAngle()
{
  int16_t ax;
  int16_t ay;
  int16_t az;
  accel.getAcceleration(&ax, &ay, &az);
  int xAng = map(ax, minAccelVal, maxAccelVal, -90, 90);
  int yAng = map(ay, minAccelVal, maxAccelVal, -90, 90);
  uint16_t crankAngle = RAD_TO_DEG * (atan2(-yAng, -xAng) + PI);
  return crankAngle;
}

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  bluetooth->initialize();

  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  loadcell.set_scale(LOADCELL_SCALE_VALUE);

  //loadcell.set_scale(); //these are for debug
  //loadcell.tare();
  // Serial.begin(115200);
  // Serial.println("INIT!");

  Wire.begin(ACCEL_SDA_PIN, ACCEL_SCL_PIN);
  accel.initialize();
}

//reading holders
int16_t powerReading = 0;           //in W
uint16_t cumulativeRevolutions = 0; //a represents the total number of times a crank rotates

uint16_t lastCET = 0; //it NEEDS to be an uint16_t for it to roll over properly
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

/*
0 = measuring
1 = transmitting
2 = already transmitting, idling

it goes 0 -> 1 -> 2 -> 0 -> 1 -> ... no backsteps
*/
int state = 2;

long loopStartTime = 0;
uint16_t crankAngle = 0;
long revPeriod = 1;

void loop()
{
  // notify changed value
  if (deviceConnected)
  {
    crankAngle = getCrankAngle();
    switch (state)
    {
    case 0: /*Measure state*/
      if (crankAngle < 270)
      {
        //todo add measuring logic here
        digitalWrite(LED_PIN, HIGH);
      }
      else
      {
        // Serial.println("0->1");
        state = 1;
      }
      break;
    case 1: /*Transmit state*/
      revPeriod = (millis() - loopStartTime) * 2; //times 2 because i only measured 180 degrees ha
      cumulativeRevolutions++;
      lastCET += revPeriod * 1000 / 1024;
      bluetooth->sendPower(revPeriod);
      bluetooth->sendCSC(lastCET, cumulativeRevolutions);

      // Serial.println("1->2");
      state = 2;
      break;
    case 2: /*All done state*/
      digitalWrite(LED_PIN, LOW);
      if (crankAngle > 90 && crankAngle < 105)
      {
        loopStartTime = millis();
        state = 0;
        // Serial.print("2->0");
      }
      break;
    }
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