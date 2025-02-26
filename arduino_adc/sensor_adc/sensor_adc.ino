#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
volatile bool deviceConnected = false;
volatile bool oldDeviceConnected = false;
float adcValue[3];
float firstValue[3];

#define SERVICE_UUID        "5fafc201-1fb5-459e-8fcc-c5c9c331914c"
#define CHARACTERISTIC_UUID "ceb5483e-36e1-4688-b7f5-ea07361b26a9"

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Client connected!");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Client disconnected, restarting advertisement...");
    }
};

void Read_AD(float *a) {//差分平均
  const int numReadings = 16;
  const int Shift = 4;
  static int readings[3][numReadings] = {0};
  static int readIndex = 0;
  static int total[3] = {0, 0, 0};

  int analogPins[3] = {A0, A1, A2}; // ESP32ピン番号

  for (int i = 0; i < 3; i++) {
    total[i] -= readings[i][readIndex];
    readings[i][readIndex] = analogRead(analogPins[i]);
    total[i] += readings[i][readIndex];
  }
  
  readIndex++;
  if (readIndex >= numReadings) {
    readIndex = 0;
  }
  
  for (int i = 0; i < 3; i++) {
    a[i] = total[i] >> Shift;
  }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE server...");
    
    BLEDevice::init("NanoESP32_sensor");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ |
                        BLECharacteristic::PROPERTY_NOTIFY
                      );

    BLE2902 *desc = new BLE2902();
    desc->setNotifications(true);  
    pCharacteristic->addDescriptor(desc);

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();
    Serial.println("BLE Advertising started...");
    firstValue[0] = analogRead(A0);
    firstValue[1] = analogRead(A1);
    firstValue[2] = analogRead(A2);
}

void loop() {
    Read_AD(adcValue);
    adcValue[0] -= firstValue[0];
    adcValue[1] -= firstValue[1];
    adcValue[2] -= firstValue[2];

    Serial.printf("ADC値: %.2f, %.2f, %.2f", adcValue[0], adcValue[1], adcValue[2]);

    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        Serial.println("Restarting BLE Advertising...");
        BLEDevice::startAdvertising();
    }
    oldDeviceConnected = deviceConnected;

     if (deviceConnected) {
        pCharacteristic->setValue((uint8_t*)&adcValue, sizeof(adcValue));
        pCharacteristic->notify();
    }

    delay(10);//ms
}
