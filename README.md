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
| [esp-mqtt](https://github.com/256dpi/esp-mqtt)|3.1|MIT|11kB (113kB + ?)| No (ESP32)|
| [esp-mqtt](https://github.com/espressif/esp-mqtt)|3.1|Apache 2.0|12kB (115kb + ?)| No (ESP32)|
| [wolfMQTT](https://github.com/wolfSSL/wolfMQTT)|5.0|GPL 2.0|not tested due to license|Yes (Posix+Win32+Arduino)|
| eMQTT5|5.0|MIT||Yes (Posix+Win32+Lwip(for ex: ESP32))|

## API Documentation


## Porting to a new platform


