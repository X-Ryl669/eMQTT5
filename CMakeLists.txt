cmake_minimum_required(VERSION 3.7)

# Fix behavior of CMAKE_CXX_STANDARD when targeting macOS.
if (POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif ()

project(eMQTT5)

option(REDUCED_FOOTPRINT "Whether to enable reduced footprint for the client code" ON)
option(CROSSPLATFORM_SOCKET "Whether to use cross plaftform socket code (this disable SSL)" OFF)
option(ENABLE_TLS "Whether to enable TLS/SSL code (you'll need MBedTLS available)" OFF)

if (CROSSPLATFORM_SOCKET STREQUAL OFF AND ENABLE_TLS STREQUAL ON)
   message(WARNING "As of 06/28/2020, MBedTLS is not correctly CMake compatible and does not generate a mbedtls-config.cmake file. You'll need to apply the patch from my branch found in pull request #3465")  
   find_package(mbedtls CONFIG REQUIRED)
endif ()

add_subdirectory(lib)
add_subdirectory(tests)

