add_executable(MQTTc
    MQTTc.cpp
    ClassPath/src/Address.cpp
    ClassPath/src/bstrwrap.cpp
    ClassPath/src/HashKey.cpp
    ClassPath/src/Lock.cpp
    ClassPath/src/Logger.cpp
    ClassPath/src/Strings.cpp
    ClassPath/src/Time.cpp
    ClassPath/src/Socket.cpp
    ClassPath/src/LinuxSpecific.cpp
    ClassPath/src/Dump.cpp

    ClassPath/src/bstrlib.c)

add_executable(MQTTParsePacket
    MQTTParsePacket.cpp
    ClassPath/src/bstrwrap.cpp
    ClassPath/src/HashKey.cpp
    ClassPath/src/Lock.cpp
    ClassPath/src/Logger.cpp
    ClassPath/src/Strings.cpp
    ClassPath/src/Time.cpp
    ClassPath/src/LinuxSpecific.cpp
    ClassPath/src/Dump.cpp

    ClassPath/src/bstrlib.c)

add_executable(SerializationTests
    SerializationTests.cpp
    ClassPath/src/bstrwrap.cpp
    ClassPath/src/HashKey.cpp
    ClassPath/src/Lock.cpp
    ClassPath/src/Logger.cpp
    ClassPath/src/Strings.cpp
    ClassPath/src/Time.cpp
    ClassPath/src/LinuxSpecific.cpp
    ClassPath/src/Dump.cpp

    ClassPath/src/bstrlib.c)



set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set_target_properties(MQTTc PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

set_target_properties(MQTTParsePacket PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

include_directories(ClassPath/include/)

target_compile_definitions(MQTTc PUBLIC _DEBUG=$<CONFIG:Debug>
                                        MinimalFootPrint=$<STREQUAL:${REDUCED_FOOTPRINT},ON>
                                        MQTTOnlyBSDSocket=$<STREQUAL:${CROSSPLATFORM_SOCKET},OFF>
                                        MQTTUseTLS=$<AND:$<STREQUAL:${CROSSPLATFORM_SOCKET},OFF>,$<STREQUAL:${ENABLE_TLS},ON>>)

target_compile_definitions(MQTTParsePacket PUBLIC _DEBUG=$<CONFIG:Debug>
                                        $<$<PLATFORM_ID:Linux>:_LINUX=1>)

target_compile_definitions(SerializationTests PUBLIC _DEBUG=$<CONFIG:Debug>
                                        $<$<PLATFORM_ID:Linux>:_LINUX=1>)


IF(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  IF (WIN32)
  ELSE()
    target_compile_options(MQTTc PUBLIC -fno-rtti -ffunction-sections -fdata-sections)
    target_link_options(MQTTc PUBLIC $<$<CXX_COMPILER_ID:GNU>:--gc-sections,--as-needed>)
    target_link_options(MQTTc PUBLIC $<$<CXX_COMPILER_ID:Clang>:-dead_strip>)
    target_link_options(MQTTc PUBLIC $<$<CXX_COMPILER_ID:AppleClang>:-dead_strip>)
  ENDIF()
ENDIF()


IF (CROSSPLATFORM_SOCKET STREQUAL OFF AND ENABLE_TLS STREQUAL ON)
    target_link_libraries(MQTTc LINK_PRIVATE mbedtls mbedcrypto mbedx509)
ENDIF()


target_link_libraries(MQTTc LINK_PUBLIC eMQTT5 ${CMAKE_DL_LIBS} Threads::Threads)
install(TARGETS MQTTc RUNTIME DESTINATION bin)
target_link_libraries(MQTTParsePacket LINK_PUBLIC eMQTT5 ${CMAKE_DL_LIBS} Threads::Threads)
install(TARGETS MQTTParsePacket RUNTIME DESTINATION bin)

target_link_libraries(SerializationTests LINK_PUBLIC eMQTT5 ${CMAKE_DL_LIBS} Threads::Threads)

