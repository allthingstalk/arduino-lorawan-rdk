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
 *  AllThingsTalk Maker Cloud IoT experiment for LoRa
 *  Version 1.0 dd 09/11/2015
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
 * If the accelerometer senses movement, the device will send out GPS
 * coordinates. If we have our lock turned on, movement will trigger a
 * notification which will be visible in the AllthingsTalk Developer
 * Cloud and on our Smartphone. If the lock is turned off, nothing will
 * happen, regardless of any movement of the device.
 **/

#include <Wire.h>
#include <SoftwareSerial.h>
#include <MMA7660.h>
#include <ATT_IOT_LoRaWAN.h>
#include "keys.h"
#include <MicrochipLoRaModem.h>
#include <Container.h>

#define Serial_BAUD 57600

#define debugSerial Serial
#define loraSerial Serial1

#define MOVEMENTTRESHOLD 12  // amount of movement that can be detected before being considered as really moving (jitter on the accelerometer, vibrations)

MicrochipLoRaModem Modem(&loraSerial, &debugSerial);
ATTDevice Device(&Modem, &debugSerial, false, 7000);  // minimum time between 2 messages set at 7000 milliseconds

Container container(Device);

MMA7660 accelemeter;

SoftwareSerial SoftSerial(20, 21);  // reading GPS values from debugSerial connection with GPS

unsigned char buffer[64];  // buffer array for data receive over debugSerial port
int count=0;  

// variables for the coordinates (GPS)
float latitude;
float longitude;
float altitude;
float timestamp;

int8_t prevX,prevY,prevZ;  // keeps track of the accelerometer data that was read last previously, so we can detect a difference in position

// accelerometer data is translated to 'moving vs not moving' on the device
// this boolean value is sent to the cloud using a generic 'Binary Sensor' container
bool wasMoving = false;     

void sendVal(boolean val)
{
  container.AddToQueue(val, BINARY_SENSOR, false);
  Device.ProcessQueue();
  while(Device.ProcessQueue() > 0) {
    debugSerial.print("QueueCount: "); debugSerial.println(Device.QueueCount());
    delay(10000);
  }
}

void setup() 
{
  accelemeter.init();  // accelerometer is always running so we can check when the object is moving around or not
  SoftSerial.begin(9600); 

  debugSerial.begin(Serial_BAUD);
  while((!debugSerial) && (millis()) < 10000){}  // wait until debugSerial. bus is available
  
  loraSerial.begin(Modem.getDefaultBaudRate());  // init the baud rate of the debugSerial. connection so that it's ok for the modem
  while((!loraSerial) && (millis()) < 10000){}   // wait until debugSerial. bus is available

  while(!Device.InitABP(DEV_ADDR, APPSKEY, NWKSKEY))
  debugSerial.println("retrying...");  // initialize connection with the AllThingsTalk Developer Cloud
  debugSerial.println("Ready to send data");

  debugSerial.println("-- Guard your stuff LoRa experiment --");
  debugSerial.println();
    
  debugSerial.print("Initializing GPS");
  while(readCoordinates() == false){
    delay(1000);
    debugSerial.print(".");
  }
  debugSerial.println("Done");

  accelemeter.getXYZ(&prevX, &prevY, &prevZ);  // get the current state of the accelerometer so we can use this info in the loop as something to compare against

  debugSerial.println("Sending initial state");
  sendVal(false);

  debugSerial.println("Ready to guard your stuff");
  debugSerial.println();
}

void loop() 
{
  if(isMoving() == true)
  {   
    if(wasMoving == false)
    {
      debugSerial.println();
      debugSerial.println("Movement detected");
      debugSerial.println("-----------------");

      //optional improvement: only turn on the gps when it is used: while the device is moving
      wasMoving = true;
      sendVal(true);
      SendCoordinates();
    }
  }
  else if(wasMoving == true)
  {
    debugSerial.println();
    debugSerial.println("Movement stopped");
    debugSerial.println("----------------");
    // we don't need to send coordinates when the device has stopped moving
    // -> they will always be the same, so we can save some power
    // optional improvement: turn off the gps module
    wasMoving = false;
    sendVal(false);
    SendCoordinates();  // send over last known coordinates
  }
  delay(500);  // sample the accelerometer quickly -> not so costly
}

bool isMoving()
{
  int8_t x,y,z;
  accelemeter.getXYZ(&x, &y, &z);
  bool result = (abs(prevX - x) + abs(prevY - y) + abs(prevZ - z)) > MOVEMENTTRESHOLD;

  // when movment stops, we check 2 times, cause often, the accelerometer reports
  // for a single tick that movement has stopped, but it hasn't, the distance between
  // the 2 measurement points was just too close, so remeasure, make certain that
  // movement has really stopped
  if(result == false && wasMoving == true)
  {
    delay(800);
    accelemeter.getXYZ(&x, &y, &z);
    result = (abs(prevX - x) + abs(prevY - y) + abs(prevZ - z)) > MOVEMENTTRESHOLD;
  }
  
  if(result == true)
  {
    prevX = x;
    prevY = y;
    prevZ = z;
  }

  return result; 
}

// tries to read the gps coordinates from the text stream that was received from the gps module
// returns: true when gps coordinates were found in the input, otherwise false
bool readCoordinates()
{
  bool foundGPGGA = false;  // sensor can return multiple types of data, need to capture lines that start with $GPGGA
  if (SoftSerial.available())                     
  {
    while(SoftSerial.available())  // reading data into char array
    {
      buffer[count++]=SoftSerial.read();  // store the received data in a temp buffer for further processing later on
      if(count == 64)
        break;
    }
    foundGPGGA = count > 60 && ExtractValues();  // if we have less then 60 characters, then we have bogus input, so don't try to parse it or process the values
    clearBufferArray();                          // call clearBufferArray function to clear the stored data from the array
  }
  return foundGPGGA;
}

//sends the GPS coordinates to the cloud
void SendCoordinates()
{
  debugSerial.print("Retrieving GPS coordinates for transmission, please wait");
  // try to read some coordinates until we have a valid set. Every time we
  // fail, pause a little to give the GPS some time. There is no point to
  // continue without reading gps coordinates. The bike was potentially
  // stolen, so the lcoation needs to be reported before switching back to
  // none moving.
  while(readCoordinates() == false){
    debugSerial.print(".");
    delay(1000);                 
  }
  debugSerial.println();
         
  container.AddToQueue(latitude, longitude, altitude, timestamp, GPS, false);  // No ACK
  Device.ProcessQueue();
  while(Device.ProcessQueue() > 0) {
    debugSerial.print("QueueCount: "); debugSerial.println(Device.QueueCount());
    delay(10000);
  }
  
  debugSerial.print("lng: ");
  debugSerial.print(longitude, 4);
  debugSerial.print(", lat: ");
  debugSerial.print(latitude, 4);
  debugSerial.print(", alt: ");
  debugSerial.print(altitude);
  debugSerial.print(", time: ");
  debugSerial.println(timestamp);
}

// extracts all the coordinates from the stream that was received from the module
// and stores the values in the globals defined at the top of the sketch
bool ExtractValues()
{
  unsigned char start = count;
  while(buffer[start] != '$')  // find the start of the GPS data -> multiple $GPGGA can appear in 1 line, if so, need to take the last one
  {
    if(start == 0)  // it's unsigned char, so we can't check on <= 0
      break;
    start--;
  }
  start++;  // remove the '$', don't need to compare with that

  // we found the correct line, so extract the values
  if(start + 4 < 64 && buffer[start] == 'G' && buffer[start+1] == 'P' && buffer[start+2] == 'G' && buffer[start+3] == 'G' && buffer[start+4] == 'A')
  {
    start += 6;
    timestamp = ExtractValue(start);
    latitude = ConvertDegrees(ExtractValue(start) / 100);
    start = Skip(start);    
    longitude = ConvertDegrees(ExtractValue(start)  / 100);
    start = Skip(start);
    start = Skip(start);
    start = Skip(start);
    start = Skip(start);
    altitude = ExtractValue(start);
    return true;
  }
  else
    return false;
}

float ConvertDegrees(float input)
{
  float fractional = input - (int)input;
  return (int)input + (fractional / 60.0) * 100.0;
}


// extracts a single value out of the stream received from the device and returns this value
float ExtractValue(unsigned char& start)
{
  unsigned char end = start + 1;
  while(end < count && buffer[end] != ',')  // find the start of the GPS data -> multiple $GPGGA can appear in 1 line, if so, need to take the last one
    end++;
  buffer[end] = 0;  // end the string, so we can create a string object from the sub string -> easy to convert to float
  float result = 0.0;
  if(end != start + 1)  // if we only found a ',' then there is no value
    result = String((const char*)(buffer + start)).toFloat();
  start = end + 1;
  return result;
}

// skips a position in the text stream that was received from the gps
unsigned char Skip(unsigned char start)
{
  unsigned char end = start + 1;
  while(end < count && buffer[end] != ',')  // find the start of the GPS data -> multiple $GPGGA can appear in 1 line, if so, need to take the last one
    end++;
  return end+1;
}

void clearBufferArray()
{
  for (int i=0; i<count;i++) buffer[i]=NULL;  // reset the entire buffer back to 0
  count = 0;
}
