add_executable(MQTTc 
    MQTTc.cpp
    ClassPath/src/Address.cpp
    ClassPath/src/bstrwrap.cpp
    ClassPath/src/HashKey.cpp
    ClassPath/src/Lock.cpp
    ClassPath/src/Logger.cpp
    ClassPath/src/Strings.cpp
    ClassPath/src/Time.cpp
    $<IF:$<PLATFORM_ID:WIN32>,ClassPath/src/Windows.cpp,ClassPath/src/Posix.cpp>

    ClassPath/src/bstrlib.c)

set_target_properties(MQTTc PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

include_directories(ClassPath/include/)

target_compile_definitions(MQTTc PUBLIC _DEBUG=$<CONFIG:Debug> MinimalFootPrint=$<STREQUAL:${REDUCED_FOOTPRINT},ON>)

IF(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  IF (WIN32)
  ELSE()
    target_compile_options(MQTTc PUBLIC -fno-rtti -ffunction-sections -fdata-sections)
    target_link_options(MQTTc PUBLIC $<$<CXX_COMPILER_ID:GNU>:--gc-sections,--as-needed>)
    target_link_options(MQTTc PUBLIC $<$<CXX_COMPILER_ID:Clang>:-dead_strip>)
    target_link_options(MQTTc PUBLIC $<$<CXX_COMPILER_ID:AppleClang>:-dead_strip>)
  ENDIF()
ENDIF()


target_link_libraries(MQTTc LINK_PUBLIC eMQTT5)