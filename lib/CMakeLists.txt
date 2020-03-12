add_library(eMQTT5 src/Network/Clients/MQTTClient.cpp)

set_target_properties(eMQTT5 PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_compile_definitions(eMQTT5 PUBLIC _DEBUG=$<CONFIG:Debug> MinimalFootPrint=$<STREQUAL:${REDUCED_FOOTPRINT},ON>)

IF (WIN32)
ELSE()
  target_compile_options(eMQTT5 PUBLIC $<$<CONFIG:MinSizeRel>:-fno-rtti -ffunction-sections -fdata-sections>)
ENDIF()

target_include_directories(eMQTT5 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)