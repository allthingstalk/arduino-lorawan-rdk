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

/****
 *  AllThingsTalk Maker  IoT experiment for LoRa
 *  Version 2.0 dd 29/06/2017
 *  Original author: Jan Bogaerts 2015
 *
 *  This sketch is part of the AllThingsTalk LoRa rapid development kit
 *  -> http://www.allthingstalk.com/lora-rapid-development-kit
 *
 *  This example sketch is based on the Proxilmus IoT network in Belgium
 *  The sketch and libs included support the
 *  - MicroChip RN2483 LoRa module
 *  - Embit LoRa modem EMB-LR1272
 *  
 *  For more information, please check our documentation
 *  -> http://allthingstalk.com/docs/tutorials/lora/setup
 *
 * Explanation:
 * 
 * Each time the door opens a counter is incremented locally on your LoRa device.
 * Every 30 seconds, if the count has changed, it will be sent to your AllthingsTalk account.
 * As soon as a count of 20 is reached, a notification is sent out to remind you that cleaning is in order.
 * A pushbutton on the device allows you to reset the count when cleaning is done.
 * This can also be seen as validation that the cleaning crew has actually visited the facility.
 *
 **/

#include <Wire.h>
#include <ATT_IOT_LoRaWAN.h>
#include "keys.h"
#include <MicrochipLoRaModem.h>
#include <Container.h>

#define SERIAL_BAUD 57600

#define debugSerial Serial
#define loraSerial Serial1

int pushButton = 20;          
int doorSensor = 4;
MicrochipLoRaModem Modem(&loraSerial, &debugSerial);
ATTDevice Device(&Modem, &debugSerial, false, 7000);  // Min Time between 2 consecutive messages set @ 7 seconds

Container container(Device);

#define SEND_MAX_EVERY 30000                                // the mimimum time between 2 consecutive updates of visit counts that are sent to the cloud (can be longer, if the value hasn't changed)

bool prevButtonState;
bool prevDoorSensor;
short visitCount = 0;                                       // keeps track of the nr of visitors 
short prevVisitCountSent = 0;
unsigned long lastSentAt = 0;                               // the time when the last visitcount was sent to the cloud.        

void setup() 
{
  pinMode(pushButton, INPUT);                               // initialize the digital pin as an input.          
  pinMode(doorSensor, INPUT);

  debugSerial.begin(SERIAL_BAUD);
  while((!debugSerial) && (millis()) < 10000){}         //wait until serial bus is available
  
  loraSerial.begin(Modem.getDefaultBaudRate());          // init the baud rate of the serial connection so that it's ok for the modem
  while((!loraSerial) && (millis()) < 10000){}         //wait until serial bus is available

  while(!Device.InitABP(DEV_ADDR, APPSKEY, NWKSKEY))
  debugSerial.println("retrying...");            // initialize connection with the AllThingsTalk Developer Cloud
  debugSerial.println("Ready to send data");

  debugSerial.println("-- Count visits LoRa experiment --");
  debugSerial.print("Sending data every ");debugSerial.print(SEND_MAX_EVERY);debugSerial.println(" milliseconds, if changed");
  debugSerial.println();
  
  prevButtonState = digitalRead(pushButton);                // set the initial state
  prevDoorSensor = digitalRead(doorSensor);                 // set the initial state
}

void sendVisitCount(short val)
{
  container.AddToQueue(val, INTEGER_SENSOR, false);         //without ACK
  Device.ProcessQueue();
  while(Device.ProcessQueue() > 0) {
    debugSerial.print("QueueCount: "); debugSerial.println(Device.QueueCount());
    delay(10000);
    }
  lastSentAt = millis();
}


void loop() 
{

  processButton();
  processDoorSensor();
  delay(100);
  if(prevVisitCountSent != visitCount && lastSentAt + SEND_MAX_EVERY <= millis())  // only send a message when something has changed and SEND_MAX_EVERY has been exceeded
    sendVisitCount(visitCount);
}



// check the state of the door sensor
void processDoorSensor()
{
  bool sensorRead = digitalRead(doorSensor);                         
  if(prevDoorSensor != sensorRead)
  {
    prevDoorSensor = sensorRead;
    if(sensorRead == true)                                // door was closed, so increment the counter 
    {
        debugSerial.println("Door closed");
        visitCount++;                                    // the door was opened and closed again, so increment the counter
		debugSerial.print("VisitCount: ");debugSerial.println(visitCount);
    }
    else
        debugSerial.println("Door open");
  }
}

void processButton()
{
  bool sensorRead = digitalRead(pushButton);            // check the state of the button
  if (prevButtonState != sensorRead)                    // verify if value has changed
  {
     prevButtonState = sensorRead;
     if(sensorRead == true)                                         
     {
        debugSerial.println("Button pressed, counter reset");
        visitCount = 0;
     }
     else
        debugSerial.println("Button released");
  }
}



