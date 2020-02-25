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


BLEServer* pServer = NULL;

BLECharacteristic* pCharacteristicPower = NULL; //the power reading itself
BLECharacteristic* pCharacteristicSensorPos = NULL;
BLECharacteristic* pCharacteristicPowerFeature = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;

uint16_t flags = 0b0000000000000000;
int16_t instPower = 0; //the power reading
uint32_t value = 0; //the ready to set characteristic value


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

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristicPower->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  byte posvalue  = 6; // right crank
  pCharacteristicSensorPos->setValue((uint8_t*)&posvalue,1);

  //for now keep it simple i can add the vectoring later
  uint32_t powerFeature = 0b0; //just 32 old zeroes
  pCharacteristicPowerFeature->setValue((uint8_t*) &powerFeature, 4);

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(CYCLING_POWER_SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
    // notify changed value
    if (deviceConnected) {
        value = (instPower << 16) | flags; //very inefficient but just for readability

        pCharacteristicPower->setValue( (uint8_t*) &value,4); 

        pCharacteristicPower->notify();
        Serial.print("Sent power value: ");
        Serial.println(instPower);
        instPower+=3;
        delay(1000); // the min is 3ms according to official docs
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