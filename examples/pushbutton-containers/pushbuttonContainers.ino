/*
  Copyright 2015-2017 AllThingsTalk

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*/

#include <ATT_IOT_LoRaWAN.h>
#include "keys.h"
#include <MicrochipLoRaModem.h>
#include <Container.h>

#define SERIAL_BAUD 57600

#define debugSerial Serial
#define loraSerial Serial1

MicrochipLoRaModem Modem(&loraSerial, &debugSerial);
ATTDevice Device(&Modem, &debugSerial, false, 7000);  // Min Time between 2 consecutive messages set @ 7 seconds
Container container(Device);

int DigitalSensor = 20;                                        // digital sensor is connected to pin D20/21

void setup() 
{  
  pinMode(DigitalSensor, INPUT);                      // initialize the digital pin as an input.          
  delay(3000);
  
  debugSerial.begin(SERIAL_BAUD);
  while((!debugSerial) && (millis()) < 10000){}         //wait until serial bus is available
  
  Serial1.begin(Modem.getDefaultBaudRate());          // init the baud rate of the serial connection so that it's ok for the modem
  while((!loraSerial) && (millis()) < 10000){}         //wait until serial bus is available
  
  while(!Device.InitABP(DEV_ADDR, APPSKEY, NWKSKEY))
  debugSerial.println("retrying...");            // initialize connection with the AllThingsTalk Developer Cloud
  debugSerial.println("Ready to send data");

  // Init Binary Sensor
  SendValue(false);
}

void SendValue(bool val)
{
  container.AddToQueue(val, BINARY_SENSOR);
  Device.ProcessQueue();
  
  while(Device.ProcessQueue() > 0) {
    debugSerial.print("QueueCount: ");
    debugSerial.println(Device.QueueCount());
    delay(10000);
  }
}

bool sensorVal = false;
bool prevButtonState = false;

void loop() 
{
  bool sensorRead = digitalRead(DigitalSensor);     // read status Digital Sensor
  if (sensorRead == 1 && prevButtonState == false)  // verify if value has changed
  {
    prevButtonState = true;
    debugSerial.println("Button pressed");
    SendValue(!sensorVal);
    sensorVal = !sensorVal;
  }
  else if(sensorRead == 0)
    prevButtonState = false;
}