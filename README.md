# arduino-lorawan-rdk

This repository contains examples and experiment sketches for the AllThingsTalk LoRaWAN Rapid Development Kit.

> It is to be used in conjunction with the [arduino-lorawan-sdk](https://github.com/allthingstalk/arduino-lorawan-sdk) which contains all basic functionality.

Some external libraries are included in this repository for use of all sensors
* [AirQuality2](https://github.com/MikeHg/AirQualitySensor/tree/d6cadaf21c6beae99fdd65bb037424ce6f855db1)
* [MMA7660](https://github.com/Seeed-Studio/Accelerometer_MMA7660) (Accelerometer)
* [TPH2](http://support.sodaq.com/sodaq-one/tph-v2/) Adafruit_Sensor and Adafruit_BME280

## Hardware

This library has been developed for:

- Sodaq Mbili
- Microchip LoRa modem

## Installation

Download the source code and copy the content of the zip file to your arduino libraries folder (usually found at /libraries) _or_ import the .zip file directly using the Arduino IDE.

## Example sketches

* `push-button-containers` basic push button example using preset [container definitions](http://docs.allthingstalk.com/developers/data/default-payload-conversion/)
* `push-button-payload-builder` basic push button example using our [custom binary payload decoding](http://docs.allthingstalk.com/developers/data/custom-payload-conversion/)
* `guard-your-stuff` lock your object and track its movement when it gets stolen
* `count-visits` count visits for facility maintanence
* `environmental-sensing` measure your environment