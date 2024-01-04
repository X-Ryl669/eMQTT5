// Usual programs
#include <stdio.h>
#include <stdlib.h>

// We need FastString
#include "Strings/Strings.hpp"
// We need MQTT client
// Swap our own string class here and don't depend on the selected build flags
#define MQTTDumpCommunication 1
#undef MQTTString
#define MQTTString Strings::FastString
#undef MQTTROString
#define MQTTROString Strings::VerySimpleReadOnlyString
#undef MQTTStringGetData
#define MQTTStringGetData(X)    X.getData()
#undef MQTTStringGetLength
#define MQTTStringGetLength(X)  X.getLength()
#include "Protocol/MQTT/MQTT.hpp"

typedef Strings::FastString String;


int main(int argc, char ** argv)
{
    Protocol::MQTT::Common::VBInt a;
    uint8 buffer[4] = {};

    printf("Testing all variable length int serialization\n");
    for (uint32 i = 0; i < Protocol::MQTT::Common::VBInt::MaxPossibleSize; i++)
    {
        a = i;
        uint32 s = a.copyInto(buffer);
        if (s > 4)
        {
            printf("Failed to serialize to VBInt %u\n", i);
            return 1;
        }
        Protocol::MQTT::Common::VBInt b;
        uint32 r = b.readFrom(buffer, s);
        if (r != s)
        {
            printf("Failed to serialize from VBInt %u [%02X, %02X, %02X, %02X] %u/%u\n", i, buffer[0], buffer[1], buffer[2], buffer[3], r, s);
            return 1;
        }
        if ((uint32)b != i)
        {
            printf("Error in serialization of %u\n", i);
            return 1;
        }
    }
    printf("Testing invalid serialization now\n");
    buffer[0] = buffer[1] = buffer[2] = 0xFF;
    buffer[3] = 0x80;

    if (a.readFrom(buffer, 4) == 4)
    {
        printf("Failed to locate error in serialization for %u\n", (uint32)a);
        return 1;
    }


    printf("Success\n");
    return 0;
}
