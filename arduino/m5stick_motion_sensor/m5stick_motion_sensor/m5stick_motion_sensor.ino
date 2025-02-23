#include <M5StickC.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

#define SERVICE_UUID        "5fafc201-1fb5-459e-8fcc-c5c9c331914c"
#define CHARACTERISTIC_UUID "ceb5483e-36e1-4688-b7f5-ea07361b26a9"

float accX = 0, accY = 0, accZ = 0;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
        M5.Lcd.println("Connected");
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
        M5.Lcd.println("Disconnected");
    }
};

void setup() {
    M5.begin();
    M5.MPU6886.Init();
    M5.Lcd.setRotation(3);
    M5.Lcd.setTextSize(1);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Motion");

    BLEDevice::init("M5Stick_Motion");
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
}

void loop() {
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
        M5.Lcd.println("Restarting BLE...");
        delay(1000);
        BLEDevice::startAdvertising();  
    }
    oldDeviceConnected = deviceConnected;

    if (deviceConnected) {
        M5.MPU6886.getAccelData(&accX, &accY, &accZ);
        
        float accData[3] = {accX, accY, accZ};
        pCharacteristic->setValue((uint8_t*)&accData, sizeof(accData));
        pCharacteristic->notify();

        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
        M5.Lcd.println("Motion");
        M5.Lcd.printf("X:%5.2f\n", accX);
        M5.Lcd.printf("Y:%5.2f\n", accY);
        M5.Lcd.printf("Z:%5.2f", accZ);
    }
    
    delay(20);  // ← 20ms に変更
}
