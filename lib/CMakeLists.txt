add_library(eMQTT5 src/Network/Clients/MQTTClient.cpp)

set_target_properties(eMQTT5 PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

IF (${CROSSPLATFORM_SOCKET} STREQUAL "ON" AND ${ENABLE_TLS} STREQUAL "ON") 
  message(WARNING "Building crossplatform socket code with TLS is not supported yet, disabling TLS code")
ENDIF()

target_compile_definitions(eMQTT5 PUBLIC _DEBUG=$<CONFIG:Debug> 
                                        MinimalFootPrint=$<STREQUAL:${REDUCED_FOOTPRINT},ON>
                                        $<$<PLATFORM_ID:Linux>:_LINUX=1> 
                                        MQTTOnlyBSDSocket=$<STREQUAL:${CROSSPLATFORM_SOCKET},OFF> 
                                        MQTTUseTLS=$<AND:$<STREQUAL:${CROSSPLATFORM_SOCKET},OFF>,$<STREQUAL:${ENABLE_TLS},ON>>)

IF (WIN32)
ELSE()
  target_compile_options(eMQTT5 PUBLIC $<$<CONFIG:MinSizeRel>:-fno-rtti -ffunction-sections -fdata-sections>)
ENDIF()

target_include_directories(eMQTT5 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include $<$<STREQUAL:${CROSSPLATFORM_SOCKET},ON>:${CMAKE_CURRENT_SOURCE_DIR}/../tests/ClassPath/include>)

install(TARGETS eMQTT5 
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
