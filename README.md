# eMQTT5
An embedded MQTTv5 client in C++ with minimal footprint, maximal performance.

This repository contains a complete MQTT v5.0 client that's optimized for code size without sacrifying performance.

## Why another MQTT client ?
For many reasons:

- Many clients around don't support MQTT v5.0 protocol (only limited to version 3.1)
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
| eMQTT5|5.0|MIT||Yes (Posix+Win32+Lwip(for ex: ESP32))|

## API Documentation
There are two levels to access this client. The low level implies dealing with packet construction, serialization (without any network code). It's documented in the `include/Protocol/MQTT.hpp` file. 

The higher level API is documented in the `include/Network/Client/MQTT.hpp` file where you only need to call methods of the `Network::Client::MQTTv5` class (all serialization is done for you).

In all cases, almost all methods avoid allocating memory on the heap (stack is prefered whenever possible).
There is only few places where heap allocations are performed and they are documented in there respective documentation.

Typically, user-generated `Properties` are allocating on the heap (in your code, not here) and user-generated `SubscribeTopic` are also allocating on the heap (in your code, not here). 

An example software is provided that's implementing a complete MQTTv5 client in `MQTTc.cpp` where you can subscribe/publish to a topic. This file, once built on a Linux AMD64 system takes 151kB of binary space without any dependencies.

## Porting to a new platform
The implementation for a new platform is very quick. 

The only dependencies for this client rely on a `Lock` class to protect again multithreading access/reentrancy and a `ScopedLock` RAII class for acquiring and releasing the lock upon scope leaving. A default spinlock class is provided in the minimal implementation.

BSD socket API is used with only minimum feature set (only `recv`, `send`, `getaddrinfo`, `select`, `socket`, `close/closesocket`, `setsockopt` is required).  

The only options used for socket (optional, can be disabled) are: `TCP_NODELAY`, `fcntl/O_NONBLOCK`
If your platform supports `SO_RCVTIMEO/SO_SNDTIMEO` then the binary code size can be reduced even more.

Please check the `MQTTClient.cpp` file for two different examples of platform support (from complete, deterministic, Posix based implementation to simplest embedded system)

