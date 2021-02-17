CMAKE_MINIMUM_REQUIRED(VERSION 3.19)
# set(CMAKE_VERBOSE_MAKEFILE 1) # for verbose mode
PROJECT(eMQTT5)

ADD_LIBRARY(eMQTT5 STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/lib/src/Network/Clients/MQTTClient.cpp
	)
TARGET_COMPILE_FEATURES(eMQTT5 PUBLIC cxx_constexpr)
# See compile features: http://www.cmake.org/cmake/help/v3.1/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall")
SET_PROPERTY(TARGET eMQTT5 PROPERTY CXX_STANDARD 20)

TARGET_INCLUDE_DIRECTORIES(eMQTT5
	PRIVATE # Only this lib (but users of this lib) refers these private include
		${CMAKE_CURRENT_SOURCE_DIR}/tests/ClassPath/include
	PUBLIC  # Both this lib and users of this lib refer these public include
		${CMAKE_CURRENT_SOURCE_DIR}/lib/include
	)
