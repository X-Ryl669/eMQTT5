cmake_minimum_required(VERSION 3.7)

# Fix behavior of CMAKE_CXX_STANDARD when targeting macOS.
if (POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif ()


# Not using project properties here since it doesn't work in 3.7 version here
project(eMQTT5)

option(REDUCED_FOOTPRINT "Whether to enable reduced footprint for the client code" ON)
option(CROSSPLATFORM_SOCKET "Whether to use cross plaftform socket code (this disable SSL)" OFF)
option(ENABLE_TLS "Whether to enable TLS/SSL code (you'll need MBedTLS available)" OFF)
option(LOW_LATENCY "Whether to enable low latency code (at the cost of higher CPU usage)" OFF)

if (CROSSPLATFORM_SOCKET STREQUAL OFF AND ENABLE_TLS STREQUAL ON)
   message(WARNING "As of 06/28/2020, MBedTLS is not correctly CMake compatible and does not generate a mbedtls-config.cmake file. You'll need to apply the patch from my branch found in pull request #3465")  
   find_package(mbedtls CONFIG REQUIRED)
endif ()



add_subdirectory(lib)
add_subdirectory(tests)

# This is for generating the pkg-config file to install
set(lib1 eMQTT5)
set(pc_req_public)
set(emqtt5ver 1.0.0)
set(emqtt5url "https://blog.cyril.by/fr/documentation/emqtt5-doc/emqtt5")
set(emqtt5desc "An feature complete and small embbedded MQTTv5 client")
configure_file(eMQTT5.pc.in eMQTT5.pc @ONLY)  
