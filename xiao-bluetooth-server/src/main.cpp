#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <stdlib.h>

// HC-SR04 Pin definitions
const int trigPin = D3; // Trig pin connected to D3
const int echoPin = D2; // Echo pin connected to D2

// BLE variables
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long previousMillis = 0;
const long interval = 1000;

// Sensor readings variables
float rawDistance = 0;
float filteredDistance = 0;
const int numReadings = 10;
float readings[numReadings]; // the readings from the sensor
int readIndex = 0; // the index of the current reading
float total = 0; // the running total
float average = 0; // the average

#define SERVICE_UUID        "f7a21193-f1ad-443d-9b65-e752f970f35f"
#define CHARACTERISTIC_UUID "bd29c9f6-7bbd-4ad2-acbb-7d7b9c1cf3ec"

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    // HC-SR04 Sensor setup
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    for (int thisReading = 0; thisReading < numReadings; thisReading++) {
        readings[thisReading] = 0;
    }

    // BLE setup
    BLEDevice::init("CECILIA_ESP32S3");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); 
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop() {
    // Reading the sensor
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    long duration = pulseIn(echoPin, HIGH);
    rawDistance = duration * 0.034 / 2; // Speed of sound wave divided by 2 (go and back)

    // Moving Average Filter
    total = total - readings[readIndex];
    readings[readIndex] = rawDistance;
    total = total + readings[readIndex];
    readIndex = readIndex + 1;

    if (readIndex >= numReadings) {
        readIndex = 0;
    }

    average = total / numReadings;
    filteredDistance = average; 

    Serial.print("Raw Distance: ");
    Serial.print(rawDistance);
    Serial.print("\tFiltered Distance: ");
    Serial.println(filteredDistance);
    //Serial.print("\tDuration: ");
    //Serial.println(duration);

    // BLE Transmission
    if (deviceConnected && filteredDistance < 30) { // Only transmit if filtered distance is less than 30 cm
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            String message = "Distance: " + String(filteredDistance);
            pCharacteristic->setValue(message.c_str());
            pCharacteristic->notify();
            Serial.println("Notify value: " + message);
            previousMillis = currentMillis;
        }
    }

    // Disconnecting and connecting logic
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    delay(800);
}
