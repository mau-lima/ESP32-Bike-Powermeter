#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>

#define CYCLING_POWER_SERVICE_UUID    "00001818-0000-1000-8000-00805F9B34FB"
#define POWER_CHARACTERISTIC_UUID     "00002A63-0000-1000-8000-00805F9B34FB"
#define SENSORPOS_CHARACTERISTIC_UUID "00002A5D-0000-1000-8000-00805F9B34FB"
#define POWERFEATURE_CHARACTERISTIC_UUID "00002A65-0000-1000-8000-00805F9B34FB"
#define LED_PIN 2

#define CSC_SERVICE_UUID    "00001816-0000-1000-8000-00805F9B34FB"
#define CSC_MEASUREMENT_CHARACTERISTIC_UUID     "00002A5B-0000-1000-8000-00805F9B34FB"
#define CSC_FEATURE_CHARACTERISTIC_UUID "00002A5C-0000-1000-8000-00805F9B34FB"


BLEServer* pServer = NULL;

BLECharacteristic* pCharacteristicPower = NULL; //the power reading itself
BLECharacteristic* pCharacteristicSensorPos = NULL;
BLECharacteristic* pCharacteristicPowerFeature = NULL;


//CSC ONES
BLECharacteristic* pCharacteristicCSC = NULL; //the cadence reading
BLECharacteristic* pCharacteristicCSCFeature = NULL;


bool deviceConnected = false;
bool oldDeviceConnected = false;


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      digitalWrite(LED_PIN,HIGH);
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      digitalWrite(LED_PIN,LOW);
    }
};


void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  // Create the BLE Device
  BLEDevice::init("ESP32MauPowerMeter"); // weirdly enough names with spaces do not seem to work

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(CYCLING_POWER_SERVICE_UUID);


  //CSC SERVICE
  BLEService *CSCService = pServer->createService(CSC_SERVICE_UUID);

  // Create the needed BLE Characteristics
  pCharacteristicPower = pService->createCharacteristic(
                      POWER_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristicSensorPos = pService->createCharacteristic(
    SENSORPOS_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ
  );

  pCharacteristicPowerFeature = pService->createCharacteristic(
    POWERFEATURE_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ
  );

  //CSC CHARACTERISTICS
  pCharacteristicCSC = CSCService->createCharacteristic(
                      CSC_MEASUREMENT_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );


  pCharacteristicCSCFeature = CSCService->createCharacteristic(
    CSC_FEATURE_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ
  );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristicPower->addDescriptor(new BLE2902());

  //CSC
  pCharacteristicCSC->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  //CSC
  CSCService->start();

  byte posvalue  = 6; // right crank
  pCharacteristicSensorPos->setValue((uint8_t*)&posvalue,1);

  //ALL FEATURE SETTINGfor now keep it simple i can add the vectoring later
  uint32_t powerFeature = 0b0; //just 32 old zeroes
  pCharacteristicPowerFeature->setValue((uint8_t*) &powerFeature, 4);

  uint16_t CSCFeature = 0b010; //only crank rpms supported
  pCharacteristicCSCFeature->setValue((uint8_t*) &CSCFeature, 2);

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(CYCLING_POWER_SERVICE_UUID);//todo does it report cadence even if it is not advertised
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

//power variables
uint16_t powerFlags = 0b0000000000000000;
int16_t powerReading = 0; //the power reading
uint32_t powerValue = 0; //the ready to set characteristic value

//CSC variables
uint64_t CSCFlags = 0b10;
uint64_t cumulativeRevolutions = 0;
uint64_t lastCET = 1024; //fixed for 60rpm

uint64_t CSCValue = 0;

void loop() {
    // notify changed value
    if (deviceConnected) {
        powerValue = (powerReading << 16) | powerFlags; //very inefficient but just for readability
        pCharacteristicPower->setValue( (uint8_t*) &powerValue,4); 
        pCharacteristicPower->notify();

        CSCValue = (lastCET << 24) | (cumulativeRevolutions << 8) | CSCFlags;
        pCharacteristicCSC->setValue( (uint8_t*) &CSCValue,5); 
        pCharacteristicCSC->notify();

        //fake data generating part
        powerReading+=3;
        cumulativeRevolutions++;
        lastCET = (lastCET == 1024)?0:1024;
        //END of fake data generating part
        
        Serial.print("Power: "); Serial.print(powerReading); Serial.println("W");
        Serial.print("Cadence data: ");Serial.print((long) cumulativeRevolutions);Serial.print(" revs and LCET:" );Serial.println((long) lastCET);

        delay(1000); // the minimum is 3ms according to official docs
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}