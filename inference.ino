/*
    Gesture Recognition Glove
    --------------------------------
    Venkatesh Kondapalli   2110040017
    Sri Ram Rahul Pulipaka 2110040021
    Suryanshu Basa         2110040027


    SCL -> 22 GPIO
    SDA -> 21 GPIO

*/

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

#include <TensorFlowLite_ESP32.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>


#include "inferenceModel.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

Adafruit_MPU6050 mpu;

const float accelerationThreshold = 40;  // threshold of significant in m/s^2
const int numSamples = 150;


float mx = 0;
int idx = 0;

// TFLM global variables
tflite::MicroErrorReporter tflErrorReporter;
tflite::AllOpsResolver tflOpsResolver;
const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;

// Arena size for input , output and intermediate tensors
constexpr int tensorArenaSize = 8 * 1024;
byte tensorArena[tensorArenaSize] __attribute__((aligned(16)));

char* GESTURES[] = {
  "Forward fall gesture",
  "Left fall gesture",
  "Up auntyclock gesture",
  "Up clock gesture"
};

#define NUM_GESTURES (sizeof(GESTURES) / sizeof(GESTURES[0]))



void setup() {
  Serial.begin(115200);
  BLEDevice::init("ESP32");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();

  while (!Serial);

  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setGyroRange(MPU6050_RANGE_2000_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  delay(100);

  tflModel = tflite::GetModel(model);
  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema mismatch!");
    while (1)
      ;
  }


  tflInterpreter = new tflite::MicroInterpreter(tflModel, tflOpsResolver, tensorArena, tensorArenaSize, &tflErrorReporter);


  tflInterpreter->AllocateTensors();

  tflInputTensor = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);
}

void loop() {
  detectMotion();
}

void detectMotion() {
  float aX;
float aY;
float aZ;
float gX;
float gY;
float gZ;
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  aX = a.acceleration.x;
  aY = a.acceleration.y;
  aZ = a.acceleration.z;

  float aSum = fabs(aX) + fabs(aY) + fabs(aZ - 9.8);

  if (aSum >= accelerationThreshold) runInference();
  // else Serial.println("0");
}

void runInference() {
  float aX;
float aY;
float aZ;
float gX;
float gY;
float gZ;
  for (int samplesRead = 0; samplesRead < numSamples; samplesRead++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    aX = a.acceleration.x;
    aY = a.acceleration.y;
    aZ = a.acceleration.z;
    gX = g.gyro.x;
    gY = g.gyro.y;
    gZ = g.gyro.z;

    // normalize the IMU data between 0 to 1 and store in the model's
    // input tensor
    tflInputTensor->data.f[samplesRead * 6 + 0] = (aX + 39.2) / 78.4;
    tflInputTensor->data.f[samplesRead * 6 + 1] = (aY + 39.2) / 78.4;
    tflInputTensor->data.f[samplesRead * 6 + 2] = (aZ + 39.2) / 78.4;
    tflInputTensor->data.f[samplesRead * 6 + 3] = (gX + 20) / 40;
    tflInputTensor->data.f[samplesRead * 6 + 4] = (gY + 20) / 40;
    tflInputTensor->data.f[samplesRead * 6 + 5] = (gZ + 20) / 40;
  }

  TfLiteStatus invokeStatus = tflInterpreter->Invoke();
  if (invokeStatus != kTfLiteOk) {
    // Serial.println("Invoke failed!");
    while (1);
    return;
  }
  else
    mx = 0;
  
  // int out = 0;
  for (int i = 0; i < NUM_GESTURES; i++) {
    // Serial.print(GESTURES[i]);
    // Serial.print(": ");
    // Serial.println(tflOutputTensor->data.f[i], 6);
    float temp = tflOutputTensor->data.f[i];
    if(temp > mx){
      mx = temp;
      idx = i;
    }

  }

  Serial.println(GESTURES[idx]);
  if (deviceConnected) {
        // std::string message = "hello";
        pCharacteristic->setValue(GESTURES[idx]);
        pCharacteristic->notify();
        delay(1000); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
    }
}

// void initSensor() {
//   float totX, totY, totZ;
//   sensors_event_t a, g, temp;

//   for (int i = 0; i < 10; i++) {
//     mpu.getEvent(&a, &g, &temp);
//     totX = totX + a.acceleration.x;
//     totY = totY + a.acceleration.y;
//     totZ = totZ + a.acceleration.z;
//   }
//   baseAx = totX / 10;
//   baseAy = totY / 10;
//   baseAz = totZ / 10;
// }

// if (deviceConnected) {
//         std::string message = "hello";
//         pCharacteristic->setValue(message);
//         pCharacteristic->notify();
//         value++;
//         delay(1000); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
//     }
