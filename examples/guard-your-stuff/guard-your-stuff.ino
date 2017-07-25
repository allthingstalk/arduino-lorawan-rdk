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

/*
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
 */

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

#define MOVEMENTTRESHOLD 12  // threshold for accelerometer movement

MicrochipLoRaModem modem(&loraSerial, &debugSerial);
ATTDevice device(&modem, &debugSerial, false, 7000);  // minimum time between 2 messages set at 7000 milliseconds

Container container(device);

MMA7660 accelemeter;

SoftwareSerial SoftSerial(20, 21);  // reading GPS values from debugSerial connection with GPS

unsigned char buffer[64];  // buffer array for data receive over debugSerial port
int count=0;  

// variables for the coordinates (GPS)
float latitude;
float longitude;
float altitude;
float timestamp;

int8_t prevX,prevY,prevZ;  // keeps track of the previous accelerometer data

// accelerometer data is translated to 'moving' vs 'not moving' on the device
// this boolean value is sent to the cloud using a generic 'Binary Sensor' container
bool wasMoving = false;     

void setup() 
{
  // accelerometer is always running so we can check when the object is moving
  accelemeter.init();
  SoftSerial.begin(9600); 

  debugSerial.begin(Serial_BAUD);
  while((!debugSerial) && (millis()) < 10000){}  // wait until the serial bus is available
  
  loraSerial.begin(modem.getDefaultBaudRate());  // set baud rate of the serial connection to match the modem
  while((!loraSerial) && (millis()) < 10000){}   // wait until the serial bus is available

  while(!device.initABP(DEV_ADDR, APPSKEY, NWKSKEY))
  debugSerial.println("retrying...");  // initialize connection with the AllThingsTalk Developer Cloud
  debugSerial.println("Ready to send data");

  debugSerial.println();
  debugSerial.println("-- Guard your stuff LoRa experiment --");
  debugSerial.println();
    
  debugSerial.print("Initializing GPS");
  while(readCoordinates() == false)
  {
    delay(1000);
    debugSerial.print(".");
  }
  debugSerial.println("Done");

  accelemeter.getXYZ(&prevX, &prevY, &prevZ);  // get initial accelerometer state

  debugSerial.println("Sending initial state");
  sendVal(false);

  debugSerial.println("Ready to guard your stuff");
  debugSerial.println();
}

void process()
{
  device.processQueue();
  while(device.processQueue() > 0) {
    debugSerial.print("QueueCount: ");
    debugSerial.println(device.queueCount());
    delay(10000);
  }
}

void sendVal(boolean val)
{
  container.addToQueue(val, BINARY_SENSOR, false);
  process();
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

      wasMoving = true;
      sendVal(true);
      sendCoordinates();
    }
  }
  else if(wasMoving == true)
  {
    debugSerial.println();
    debugSerial.println("Movement stopped");
    debugSerial.println("----------------");
    
    // no need to send coordinates when the device has stopped moving
    // since the coordinates stay the same. This saves some power
    wasMoving = false;
    sendVal(false);
    sendCoordinates();  // send over last known coordinates
  }
  delay(500);  // sample the accelerometer quickly
}

bool isMoving()
{
  int8_t x,y,z;
  accelemeter.getXYZ(&x, &y, &z);
  bool result = (abs(prevX - x) + abs(prevY - y) + abs(prevZ - z)) > MOVEMENTTRESHOLD;

  // do a second measurement to make sure movement really stopped
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

// try to read the gps coordinates from the text stream that was received from the gps module
// returns true when gps coordinates were found in the input, otherwise false
bool readCoordinates()
{
  // sensor can return multiple types of data
  // we need to capture lines that start with $GPGGA
  bool foundGPGGA = false;
  if (SoftSerial.available())                     
  {
    while(SoftSerial.available())  // read data into char array
    {
      buffer[count++]=SoftSerial.read();  // store data in a buffer for further processing
      if(count == 64)
        break;
    }
    foundGPGGA = count > 60 && extractValues();  // if we have less then 60 characters, we have incomplete input
    clearBufferArray();
  }
  return foundGPGGA;
}

// send the GPS coordinates to the AllThingsTalk cloud
void sendCoordinates()
{
  debugSerial.print("Retrieving GPS coordinates for transmission, please wait");
  
  /*
   * try to read some coordinates until we have a valid set. Every time we
   * fail, pause a little to give the GPS some time. There is no point to
   * continue without reading gps coordinates. The bike was potentially
   * stolen, so the lcoation needs to be reported before switching back to
   * 'not moving'.
   */  
  
  while(readCoordinates() == false)
  {
    debugSerial.print(".");
    delay(1000);                 
  }
  debugSerial.println();
         
  container.addToQueue(latitude, longitude, altitude, timestamp, GPS, false);
  process();
  
  debugSerial.print("lng: ");
  debugSerial.print(longitude, 4);
  debugSerial.print(", lat: ");
  debugSerial.print(latitude, 4);
  debugSerial.print(", alt: ");
  debugSerial.print(altitude);
  debugSerial.print(", time: ");
  debugSerial.println(timestamp);
}

// extract all the coordinates from the stream
// store the values in the globals defined at the top of the sketch
bool extractValues()
{
  unsigned char start = count;
  
  // find the start of the GPS data
  // if multiple $GPGGA appear in 1 line, take the last one
  while(buffer[start] != '$')
  {
    if(start == 0)  // it's unsigned char, so we can't check on <= 0
      break;
    start--;
  }
  start++;  // skip the '$', don't need to compare with that

  // we found the correct line, so extract the values
  if(start + 4 < 64 && buffer[start] == 'G' && buffer[start+1] == 'P' && buffer[start+2] == 'G' && buffer[start+3] == 'G' && buffer[start+4] == 'A')
  {
    start += 6;
    timestamp = extractValue(start);
    latitude = convertDegrees(extractValue(start) / 100);
    start = skip(start);    
    longitude = convertDegrees(extractValue(start)  / 100);
    start = skip(start);
    start = skip(start);
    start = skip(start);
    start = skip(start);
    altitude = extractValue(start);
    return true;
  }
  else
    return false;
}

float convertDegrees(float input)
{
  float fractional = input - (int)input;
  return (int)input + (fractional / 60.0) * 100.0;
}

// extracts a single value out of the stream received from the device and returns this value
float extractValue(unsigned char& start)
{
  unsigned char end = start + 1;
  
  // find the start of the GPS data
  // if multiple $GPGGA appear in 1 line, take the last one
  while(end < count && buffer[end] != ',')
    end++;

  // end the string so we can create a string object from the sub string
  // easy to convert to float
  buffer[end] = 0;
  float result = 0.0;

  if(end != start + 1)  // if we only found a ',' then there is no value
    result = String((const char*)(buffer + start)).toFloat();

  start = end + 1;
  return result;
}

// skip a position in the text stream that was received from the gps
unsigned char skip(unsigned char start)
{
  unsigned char end = start + 1;
  
  // find the start of the GPS data
  // if multiple $GPGGA appear in 1 line, take the last one
  while(end < count && buffer[end] != ',')
    end++;

  return end+1;
}

// reset the entire buffer back to 0
void clearBufferArray()
{
  for (int i=0; i<count;i++)
  {
    buffer[i]=NULL;
  }
  count = 0;
}
