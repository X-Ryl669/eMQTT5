# eMQTT5
An embedded MQTTv5 client in C++ with minimal footprint, maximal performance.

[![X-Ryl669](https://circleci.com/gh/X-Ryl669/eMQTT5.svg?style=shield)](https://circleci.com/gh/X-Ryl669/eMQTT5)

This repository contains a complete MQTT v5.0 client that's optimized for code size without sacrifying performance.
This is, to my knowledge, the smallest (and complete!) MQTT v5.0 client for embedded system with a binary size down to less than 17kB on ESP32 (and less than 75kB on MacOSX). 
MQTT v5.0 is a more complex protocol than MQTT v3.1.1, with the addition of properties in each packet and authentication subsystem. 

If you wonder what MQTT is (or isn't), feel free to consult [this page](https://blog.cyril.by/en/documentation/emqtt5-doc/mqtt-minimal-knowledge).


## Why another MQTT client ?
For many reasons:

- Many clients around don't support MQTT v5.0 protocol (only limited to version 3.1.1)
- Some are large and/or requires numerous dependencies
- This code is specialized for embedded system with or without an operating system
- Many clients don't build on a linux system making debugging hard
- The license to use them is too restrictive
- Some client rely on a heap and fragment the heap quickly making usage over a long period dangerous


## Comparison with existings clients I know about
| Client | Supported MQTT version | License | Compiled code size (with dependencies) | Cross platform |
|--------|------------------------|---------|----------------------------------------|----------------|
| [256dpi esp-mqtt](https://github.com/256dpi/esp-mqtt)|3.1|MIT|11kB (113kB + ?)| No (ESP32)|
| [Espressif esp-mqtt](https://github.com/espressif/esp-mqtt)|3.1|Apache 2.0|12kB (115kb + ?)| No (ESP32)|
| [wolfMQTT](https://github.com/wolfSSL/wolfMQTT)|5.0|GPL 2.0|not tested due to license|Yes (Posix+Win32+Arduino)|
| [mosquitto](https://github.com/eclipse/mosquitto/)|5.0|EPL|large | Yes requires Posix|
| eMQTT5|5.0|MIT|<17kB (no dep)|Yes (Posix+Win32+Lwip(for ex: ESP32))|

## API Documentation

You'll find the [client API documentation here](https://blog.cyril.by/en/documentation/emqtt5-doc/emqtt5).

There are two levels to access this client. The low level implies dealing with packet construction, serialization (without any network code). It's documented [here](https://github.com/X-Ryl669/eMQTT5/blob/master/doc/APIDoc.md). 

The higher level API which is documented [here](https://github.com/X-Ryl669/eMQTT5/blob/master/doc/ClientAPI.md) is available when you only need to call methods of the `Network::Client::MQTTv5` class (all serialization is done for you).

In all cases, almost all methods avoid allocating memory on the heap (stack is prefered whenever possible).
There is only few places where heap allocations are performed and they are documented in there respective documentation.

Typically, user-generated [Properties](https://github.com/X-Ryl669/eMQTT5/blob/591050dd32b33376c3853b853cfab540edea31be/lib/include/Protocol/MQTT/MQTT.hpp#L1672) are allocating on the heap (in your code, not here) and user-generated [SubscribeTopic](https://github.com/X-Ryl669/eMQTT5/blob/591050dd32b33376c3853b853cfab540edea31be/lib/include/Protocol/MQTT/MQTT.hpp#L1938) are also allocating on the heap (in your code, not here). 

An example software is provided that's implementing a complete MQTTv5 client in [MQTTc.cpp](https://github.com/X-Ryl669/eMQTT5/blob/master/tests/MQTTc.cpp) where you can subscribe/publish to a topic. This file, once built on a Linux AMD64 system takes 80kB of binary space without any dependencies.

## Porting to a new platform
The implementation for a new platform is very quick. 

The only dependencies for this client rely on a `Lock` class to protect again multithreading access/reentrancy and a `ScopedLock` RAII class for acquiring and releasing the lock upon scope leaving. A default spinlock class is provided in the minimal implementation.

BSD socket API is used with only minimum feature set (only `recv`, `send`, `getaddrinfo`, `select`, `socket`, `close/closesocket`, `setsockopt` is required).  

The only options used for socket (optional, can be disabled) are: `TCP_NODELAY`, `fcntl/O_NONBLOCK`, `SO_SNDTIMEO`, `SO_RCVTIMEO`)

Please check the [MQTTClient.cpp](https://github.com/X-Ryl669/eMQTT5/blob/master/lib/src/Network/Clients/MQTTClient.cpp) file for two different examples of platform support (from complete, deterministic, Posix based implementation to simplest embedded system).

There is also a port for ESP32 [here](https://github.com/X-Ryl669/esp-eMQTT5).

## MQTTv5 Packet parser
In addition to the client, the tests folder contains a MQTT packet parser for MQTT v5.0. 
It's built by default and used like this:
```
$ # Give it the raw bytes from network communication and it'll dump what it means
$ ./MQTTParsePacket 30 1E 00 18 73 74 61 74 75 73 2F 59 4F 4C 54 79 79 76 75 57 58 50 5A 2F 6C 6F 67 73 00 5B 31 5D
Detected PUBLISH packet
with size: 32
PUBLISH control packet (rlength: 30)
  Header: (type PUBLISH, retain 0, QoS 0, dup 0)
  PUBLISH packet (id 0x0000): Str (24 bytes): status/YOLTyyvuWXPZ/logs
  Properties with length VBInt: 0
  Payload (length: 3)
``` 

You can also give it a file containing the capture of the network payload:
```
$ ./MQTTParsePacket -f capture.dump
Detected PUBLISH packet
with size: 32
PUBLISH control packet (rlength: 30)
  Header: (type PUBLISH, retain 0, QoS 0, dup 0)
  PUBLISH packet (id 0x0000): Str (24 bytes): status/YOLTyyvuWXPZ/logs
  Properties with length VBInt: 0
  Payload (length: 3)
``` 

