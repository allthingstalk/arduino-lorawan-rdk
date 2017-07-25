/*    _   _ _ _____ _    _              _____     _ _     ___ ___  _  __
 *   /_\ | | |_   _| |_ (_)_ _  __ _ __|_   _|_ _| | |__ / __|   \| |/ /
 *  / _ \| | | | | | ' \| | ' \/ _` (_-< | |/ _` | | / / \__ \ |) | ' <
 * /_/ \_\_|_| |_| |_||_|_|_||_\__, /__/ |_|\__,_|_|_\_\ |___/___/|_|\_\
 *                             |___/
 *
 * Copyright 2017 AllThingsTalk
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ATT_IOT_LoRaWAN.h>
#include "keys.h"
#include <MicrochipLoRaModem.h>
#include <Container.h>

#define SERIAL_BAUD 57600

#define debugSerial Serial
#define loraSerial Serial1

MicrochipLoRaModem modem(&loraSerial, &debugSerial);
ATTDevice device(&modem, &debugSerial, false, 7000);  // minimum time between 2 messages set at 7000 milliseconds

Container container(device);

int digitalSensor = 20;  // digital sensor is connected to pin D20/21

void setup() 
{  
  pinMode(digitalSensor, INPUT);  // initialize the digital pin as an input     
  delay(3000);
  
  debugSerial.begin(SERIAL_BAUD);
  while((!debugSerial) && (millis()) < 10000){}  // wait until the serial bus is available
  
  loraSerial.begin(modem.getDefaultBaudRate());  // set baud rate of the serial connection to match the modem
  while((!loraSerial) && (millis()) < 10000){}   // wait until the serial bus is available
  
  while(!device.initABP(DEV_ADDR, APPSKEY, NWKSKEY))
  debugSerial.println("retrying...");
  debugSerial.println("Ready to send data");

  // send initial value
  sendValue(false);
}

void sendValue(bool val)
{
  container.addToQueue(val, BINARY_SENSOR, false);  // without ACK
  
  device.processQueue();
  
  while(device.processQueue() > 0)
  {
    debugSerial.print("QueueCount: ");
    debugSerial.println(device.queueCount());
    delay(10000);
  }
}

bool sensorVal = false;
bool prevButtonState = false;

void loop() 
{
  bool sensorRead = digitalRead(digitalSensor);     // read status Digital Sensor
  if (sensorRead == 1 && prevButtonState == false)  // verify if value has changed
  {
    prevButtonState = true;
    debugSerial.println("Button pressed");
    sendValue(!sensorVal);
    sensorVal = !sensorVal;
  }
  else if(sensorRead == 0)
    prevButtonState = false;
}