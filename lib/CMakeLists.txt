add_library(eMQTT5 
            src/Network/Clients/MQTTClient.cpp
            $<$<PLATFORM_ID:WIN32>:src/Platform/Windows.cpp>
)

set_property(TARGET eMQTT5
             PROPERTY POSITION_INDEPENDENT_CODE ON
)

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
                                        MQTTOnlyBSDSocket=$<STREQUAL:${CROSSPLATFORM_SOCKET},OFF> 
                                        MQTTUseTLS=$<AND:$<STREQUAL:${CROSSPLATFORM_SOCKET},OFF>,$<STREQUAL:${ENABLE_TLS},ON>>
					MQTTLowLatency=$<STREQUAL:${LOW_LATENCY},ON>)

IF (WIN32)
ELSE()
  target_compile_options(eMQTT5 PUBLIC $<$<CONFIG:MinSizeRel>:-fno-rtti -ffunction-sections -fdata-sections>)
ENDIF()

target_include_directories(eMQTT5 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include $<$<STREQUAL:${CROSSPLATFORM_SOCKET},ON>:${CMAKE_CURRENT_SOURCE_DIR}/../tests/ClassPath/include>)

# In recent CMake version, the right way to do is like this, but it fails on older CMake
# list(APPEND header_files 
#             include/Platform/Platform.hpp
#             include/Platform/StackHeapBuffer.hpp
#             include/Network/Clients/MQTT.hpp
#             include/Network/Clients/MQTTConfig.hpp
#             include/Protocol/MQTT/MQTT.hpp
#             include/Types.hpp)
# 
# set_target_properties(eMQTT5 PROPERTIES PUBLIC_HEADER "${header_files}")


install(TARGETS eMQTT5 
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
#        PUBLIC_HEADER DESTINATION include/eMQTT5)


install(DIRECTORY include/ DESTINATION include/eMQTT5 FILES_MATCHING PATTERN "*.hpp")

install(FILES ${CMAKE_BINARY_DIR}/eMQTT5.pc DESTINATION lib/pkgconfig)
