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

const double crankLength = 0.1725; //in meters

BLEBridge *bluetooth = new BLEBridge();
HX711 loadcell;

// 1. HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 4;
const int LOADCELL_SCK_PIN = 3;
const double LOADCELL_SCALE_VALUE = 1650;

// class default I2C address is 0x53
// specific I2C addresses may be passed as a parameter here
// ALT low = 0x53 (default for SparkFun 6DOF board)
// ALT high = 0x1D
MPU6050 accel;

/*This function should return an angle from 0-359, whatever trig you use is fine as long as it works.
It could possibly be done with less math, i might be overthinking it
0 degrees = crank pointing up
90 degrees = crank pointing forwards (max torque)
TODO check how it works when leaning hard
*/
uint16_t crankAngle()
{
  int16_t ax;
  int16_t ay;
  int16_t az;
  accel.getAcceleration(&ax, &ay, &az);

  float atotal = sqrt((ax * ax) + (ay * ay));
  float roll = asin((float)ax / atotal) * -57.296;

  uint16_t result = 0;

  if (roll > 0)
    if (ay > 0)
      result = (uint16_t)roll; //quadrant 1
    else
      result = 180 - (uint16_t)roll; //quadrant 2
  else if (ay < 0)
    result = 180 - (uint16_t)roll; //quadrant 3
  else
    result = 360 + (uint16_t)roll; //quadrant 4

  return result;
}

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  bluetooth->initialize();

  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  loadcell.set_scale(LOADCELL_SCALE_VALUE);

  //loadcell.set_scale(); //these are for debug
  //loadcell.tare();

  Wire.begin();
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

void loop()
{
  // notify changed value
  if (deviceConnected)
  {

    while (crankAngle() < 260)
    {
      digitalWrite(LED_PIN, HIGH);
      delay(1); //spinlock to begin at approx.270 degrees!!
    }
    digitalWrite(LED_PIN, LOW);

    long loopStartTime = millis();

    while (crankAngle() >20) //generous
    {
      digitalWrite(LED_PIN, HIGH);
      delay(1);
    }
    digitalWrite(LED_PIN, LOW);

    uint64_t i = 0;
    double forceAccumulator = 0;
    while (crankAngle() <= 180)
    { //this will oversample on lower cadences
      forceAccumulator += loadcell.get_units(1);
      i++;
    }

    while (crankAngle() <= 250)
    {
      digitalWrite(LED_PIN, HIGH);
      delay(1);
    }
    digitalWrite(LED_PIN, LOW);

    long revPeriod = (millis() - loopStartTime); //in ms
    //Power = 2*Work/time = 2*avgForce/tangentialSpeed = 2*avgForce/(2*pi*cranklength/revPeriod). The 2 is because this is a single sided power meter so its only measuring one leg
    double avgForce = forceAccumulator / i;
    double tgSpeed = 2 * 3.14159 * crankLength / revPeriod;
    powerReading = 2 * avgForce / tgSpeed;

    cumulativeRevolutions += 1;
    lastCET += revPeriod * 1000 / 1024; //uhh TODO check what happens when this guy rolls over

    bluetooth->sendPower(revPeriod);
    bluetooth->sendCSC(lastCET, cumulativeRevolutions); //these are async luckily
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