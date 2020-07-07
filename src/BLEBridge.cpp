#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>
#include "globals.h"

#define CYCLING_POWER_SERVICE_UUID "00001818-0000-1000-8000-00805F9B34FB"
#define POWER_CHARACTERISTIC_UUID "00002A63-0000-1000-8000-00805F9B34FB"
#define SENSORPOS_CHARACTERISTIC_UUID "00002A5D-0000-1000-8000-00805F9B34FB"
#define POWERFEATURE_CHARACTERISTIC_UUID "00002A65-0000-1000-8000-00805F9B34FB"

// #define LED_PIN 2

#define CSC_SERVICE_UUID "00001816-0000-1000-8000-00805F9B34FB"
#define CSC_MEASUREMENT_CHARACTERISTIC_UUID "00002A5B-0000-1000-8000-00805F9B34FB"
#define CSC_FEATURE_CHARACTERISTIC_UUID "00002A5C-0000-1000-8000-00805F9B34FB"
#define powerFlags 0b0000000000000000;
#define CSCFlags 0b10; //todo is it good practice to define flags up here?




class MyServerCallbacks : public BLEServerCallbacks
{
private:

    void onConnect(BLEServer *pServer) //todo add a reference to deviceconnected
    {
        deviceConnected=true;
        // digitalWrite(LED_PIN, HIGH);
    };

    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected=false;
        // digitalWrite(LED_PIN, LOW);
    }
};

class BLEBridge
{ //a class for holding bluetooth-related code
private:
    BLEServer *pServer = NULL;
    //Power Characteristics
    BLECharacteristic *pCharacteristicPower = NULL; //the power reading itself
    BLECharacteristic *pCharacteristicSensorPos = NULL;
    BLECharacteristic *pCharacteristicPowerFeature = NULL;
    //CSC ONES
    BLECharacteristic *pCharacteristicCSC = NULL; //the cadence reading
    BLECharacteristic *pCharacteristicCSCFeature = NULL;

    uint32_t powerTxValue = 0; //the BLE-Compliant, flagged, ready to transmit power value
    uint64_t CSCTxValue = 0;   //the BLE-Compliant, flagged, ready to transmit CSC value

    //connection indicators
    bool deviceConnected = false;
    bool oldDeviceConnected = false;

public:
    BLEBridge()
    { // a constructor
    }

    void initialize()
    {
        // pinMode(LED_PIN, OUTPUT);
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
            BLECharacteristic::PROPERTY_NOTIFY);

        pCharacteristicSensorPos = pService->createCharacteristic(
            SENSORPOS_CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ);

        pCharacteristicPowerFeature = pService->createCharacteristic(
            POWERFEATURE_CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ);

        //CSC CHARACTERISTICS
        pCharacteristicCSC = CSCService->createCharacteristic(
            CSC_MEASUREMENT_CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_NOTIFY);

        pCharacteristicCSCFeature = CSCService->createCharacteristic(
            CSC_FEATURE_CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ);

        // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
        // Create a BLE Descriptor
        pCharacteristicPower->addDescriptor(new BLE2902());

        //CSC
        pCharacteristicCSC->addDescriptor(new BLE2902());

        // Start the service
        pService->start();

        //CSC
        CSCService->start();

        byte posvalue = 6; // right crank
        pCharacteristicSensorPos->setValue((uint8_t *)&posvalue, 1);

        //ALL FEATURE SETTINGfor now keep it simple i can add the vectoring later
        uint32_t powerFeature = 0b0; //just 32 old zeroes
        pCharacteristicPowerFeature->setValue((uint8_t *)&powerFeature, 4);

        uint16_t CSCFeature = 0b010; //only crank rpms supported
        pCharacteristicCSCFeature->setValue((uint8_t *)&CSCFeature, 2);

        // Start advertising
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(CYCLING_POWER_SERVICE_UUID); //todo does it report cadence even if it is not advertised
        pAdvertising->setScanResponse(false);
        pAdvertising->setMinPreferred(0x0); // set value to 0x00 to not advertise this parameter
        BLEDevice::startAdvertising();
    }

    void sendPower(int16_t powerReading)
    {
        int32_t power32 = powerReading;

        powerTxValue = (power32 << 16) | powerFlags; //very inefficient but just for readability
        pCharacteristicPower->setValue((uint8_t *)&powerTxValue, 4);
        pCharacteristicPower->notify();
    }

    void sendValueThroughPower(uint32_t debugValue){ //debug method, used because i want to get ADC readings while not plugging
                                                    //  any additional stuff to the circuit so as to not disturb the excitation voltage
                                                    //also, nRF Connect shows a 4-byte value of bytes DDCCBBAA with AA being the least significant byte as AA-BB-CC-DD
                                                    //like, the number 0x00000102 is shown as 0x02-01-00-00 
                                                    //additional testing: sending the debugValue 0x0A0B0C0D yields  "0x0D-0C-0B-0A" which confirms the above
        pCharacteristicPower->setValue((uint8_t *)&debugValue, 4);
        pCharacteristicPower->notify();
    }

    void sendCSC(uint16_t lastCET, uint16_t cumulativeRevolutions)
    {
        uint64_t lastCET64 = lastCET; //todo this might be optimizable
        uint64_t cumRev64 = cumulativeRevolutions;

        CSCTxValue = (lastCET64 << 24) | (cumRev64 << 8) | CSCFlags;
        pCharacteristicCSC->setValue((uint8_t *)&CSCTxValue, 5);
        pCharacteristicCSC->notify();
    }

    void startBroadcast()
    {
        delay(500);                  // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
    }
};

