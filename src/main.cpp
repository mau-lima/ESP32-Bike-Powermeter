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
const double crankLength = 0.1725; //in meters

BLEBridge *bluetooth = new BLEBridge();
HX711 loadcell;

// 1. HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 2;
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
uint16_t crankAngle(){
  int16_t ax;int16_t ay;int16_t az;
    accel.getAcceleration(&ax, &ay, &az);

    float atotal = sqrt((ax*ax)+(ay*ay));
    float roll = asin((float)ax/atotal)*-57.296;

    uint16_t result = 0;


    if(roll>0)
      if(ay>0)
        result = (uint16_t)roll; //quadrant 1
      else
        result = 180-(uint16_t)roll; //quadrant 2
    else
      if(ay<0)
        result = 180-(uint16_t)roll; //quadrant 3
      else
        result = 360+(uint16_t)roll; //quadrant 4
      

    return result;
  }

void setup()
{
  bluetooth->initialize();

  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  loadcell.set_scale(LOADCELL_SCALE_VALUE);


  //loadcell.set_scale(); //these are for debug
  //loadcell.tare();


  Wire.begin();
  // initialize serial communication
  // (38400 chosen because it works as well at 8MHz as it does at 16MHz, but
  // it's really up to you depending on your project)
  //Serial.begin(38400);
  // initialize device
  // Serial.println("Initializing Accelerometer");
  accel.initialize();
  // verify connection
  // Serial.println("Testing accelerometer connection...");
  // Serial.println(accel.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
}

//reading holders
int16_t powerReading = 0;           //in W
uint16_t cumulativeRevolutions = 0; //a represents the total number of times a crank rotates

uint16_t lastCET = 0;      //it NEEDS to be an uint16_t for it to roll over properly  
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
    uint64_t i = 0;
    double forceAccumulator = 0;
    uint32_t loopStartTime = millis();
    uint16_t loopStartAngle = crankAngle();
    while(crankAngle()>loopStartAngle){
      forceAccumulator += loadcell.get_units(10);
      i++;
      delay(50); // todo make it adjustable
    } //todo check if accelerometer angle hasnt changed much, then we're coasting. So report 0rpm 0w
    //todo check if user is pedaling backwards report 0w
    while(crankAngle()<loopStartAngle){
      forceAccumulator += loadcell.get_units(10);
      i++;
      delay(50); // todo make it adjustable
    }
    //todo make it calculate when you're on your off stroke (from 180 to 360)

    long revPeriod = (millis()-loopStartTime)/1000; //in seconds

    
    //Power = 2*Work/time = 2*avgForce/tangentialSpeed = 2*avgForce/(2*pi*cranklength/revPeriod). The 2 is because this is a single sided power meter so its only measuring one leg
    double avgForce = forceAccumulator/i;
    double tgSpeed = 2*3.14159*crankLength/revPeriod;
    powerReading = 2*avgForce/tgSpeed;

    cumulativeRevolutions+=1;
    lastCET+=revPeriod*1000/1024; //uhh TODO check what happens when this guy rolls over

    /* FAKE DATA BLOCK*/
    // powerReading += 3;
    // cumulativeRevolutions+=2; //hard set for 120rpm
    // lastCET+=loopDelay; //hard set for 120rpm
    /* END OF THE FAKE DATA BLOCK*/

    //BLE Transmit
    //bluetooth->sendValueThroughPower(loadcell.get_units(10)); //sends the raw uncalibrated HX711 value to read it on nRF Connect and make calibration spreadsheets

    bluetooth->sendPower(powerReading);
    bluetooth->sendCSC(lastCET, cumulativeRevolutions); //todo check if these are asynchronous

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