cmake_minimum_required(VERSION 3.7)

# Fix behavior of CMAKE_CXX_STANDARD when targeting macOS.
if (POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif ()

project(eMQTT5)

option(REDUCED_FOOTPRINT "Whether to enable reduced footprint for the client code" ON)


add_subdirectory(lib)
add_subdirectory(tests)

