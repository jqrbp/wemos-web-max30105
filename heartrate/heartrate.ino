/*
  Optical Heart Rate Detection (PBA Algorithm) using the MAX30105 Breakout
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 2nd, 2016
  https://github.com/sparkfun/MAX30105_Breakout

  This is a demo to show the reading of heart rate or beats per minute (BPM) using
  a Penpheral Beat Amplitude (PBA) algorithm.

  It is best to attach the sensor to your finger using a rubber band or other tightening
  device. Humans are generally bad at applying constant pressure to a thing. When you
  press your finger against the sensor it varies enough to cause the blood in your
  finger to flow differently which causes the sensor readings to go wonky.

  Hardware Connections (Breakoutboard to Arduino):
  -5V = 5V (3.3V is allowed)
  -GND = GND
  -SDA = A4 (or SDA)
  -SCL = A5 (or SCL)
  -INT = Not connected

  The MAX30105 Breakout can handle 5V or 3.3V I2C logic. We recommend powering the board with 5V
  but it will also run at 3.3V.
*/

#include <Wire.h>
#include "MAX30105.h"

#include "heartRate.h"
#include "spo2_algorithm.h"
#include "webUtils.h"

MAX30105 particleSensor;

const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;
bool fingerFlag = 0;

const char APID[15] = "myAccessPoint";
const char APpass[16] = "myPass1234";
const bool asAP = true;

int publishIdx = 0;
char wsbuff[100] = {0};

uint8_t spo2BuffIdx = 0;
uint8_t spo2BuffLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps

uint32_t zeroBuff[100] = {0};
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data

int32_t spo2; //SPO2 value
int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid

void calc_spo2(long _redVal, long _irVal) {
  // reset all spo2 variables value when no finger detected
  if (!fingerFlag) {
    if(spo2BuffIdx > 0) {
      spo2BuffIdx = 0;
      memcpy(irBuffer, zeroBuff, sizeof(uint32_t) * 100);
      memcpy(redBuffer, zeroBuff, sizeof(uint32_t) * 100);
      validSPO2 = 0;
      validHeartRate = 0;
    }
    return;
  }

  if(spo2BuffIdx < spo2BuffLength) {
    //read the first 100 samples, and determine the signal range
    redBuffer[spo2BuffIdx] = (uint32_t)_redVal;
    irBuffer[spo2BuffIdx] = (uint32_t)_irVal;

    if(spo2BuffIdx == spo2BuffLength - 1) {
      //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
      maxim_heart_rate_and_oxygen_saturation(irBuffer, spo2BuffLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
    }
  } else {
    if(spo2BuffIdx == spo2BuffLength) {
      // spo2BuffIdx range of 101
      //dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
      for (uint8_t i = 25; i < 100; i++)
      {
        redBuffer[i - 25] = redBuffer[i];
        irBuffer[i - 25] = irBuffer[i];
      }
    }
    redBuffer[spo2BuffIdx - spo2BuffLength + 75] = (uint32_t)_redVal;
    irBuffer[spo2BuffIdx - spo2BuffLength + 75] = (uint32_t)_irVal;
  }

  spo2BuffIdx++;
  if(spo2BuffIdx >= 125) {
    //After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, spo2BuffLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
    spo2BuffIdx = 100;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Initializing...");

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");

  particleSensor.setup(); //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED

  delay(1000);
  web_init(APID, APpass, asAP);
}

void loop()
{
  long redValue = particleSensor.getRed();
  long irValue = particleSensor.getIR();
  particleSensor.nextSample();

  web_loop();

  if (checkForBeat(irValue) == true)
  {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable

      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }

  if (irValue < 50000) {
    fingerFlag = false;
  } else {
    fingerFlag = true;
    Serial.print(F("IR="));
    Serial.print(irValue);
    Serial.print(F(", BPM="));
    Serial.print(beatsPerMinute);
    Serial.print(F(", Avg BPM="));
    Serial.print(beatAvg);

    Serial.print(F(", HR="));
    Serial.print(heartRate, DEC);

    Serial.print(F(", HRvalid="));
    Serial.print(validHeartRate, DEC);

    Serial.print(F(", SPO2="));
    Serial.print(spo2, DEC);

    Serial.print(F(", SPO2Valid="));
    Serial.println(validSPO2, DEC);

    Serial.println();
  }

  calc_spo2(redValue, irValue);

  sprintf(wsbuff, "{\"msg\":%d,\"f\":%d,\"ir\":%d,\"red\":%d,\"bpm\":%.2f,\"ave\":%d,\"hr\":%ld,\"spo\":%ld}\0",
            publishIdx++, (int)fingerFlag, (int)irValue, (int)redValue, beatsPerMinute, beatAvg, heartRate, spo2);
  ws_send_txt(wsbuff);
  
  
}
