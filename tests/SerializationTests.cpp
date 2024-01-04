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


void hexDump(const uint8 * inBuffer, size_t inSize)
{
    for (size_t i = 0; i < inSize; i++)
    {
        if (!(i%16)) printf("\n%08X ", i);
        printf("%02X ", inBuffer[i]);
    }
    printf("\n");
}

int err(const char * msg) {
    printf("%s\n", msg);
    return 1;
}

int main(int argc, char ** argv)
{
    Protocol::MQTT::Common::VBInt a;
    uint8 buffer[4] = {};

    if (argc > 1 && MQTTString(argv[1]) == "long")
    {
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
    } else
    {
        printf("Testing multiple variable length int serialization\n");
        uint32 toTest[] = { 0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455 };
        for (uint32 i = 0; i < ArrSz(toTest); i++)
        {
            a = toTest[i];
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
            if ((uint32)b != toTest[i])
            {
                printf("Error in serialization of %u\n", i);
                return 1;
            }
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

    // Testing packet serialization and deserialization
    {
        Protocol::MQTT::V5::registerAllProperties();

        // Please do not move the line below as it must outlive the packet
        Protocol::MQTT::V5::Property<uint32> maxProp(Protocol::MQTT::V5::PacketSizeMax, 2048);
        Protocol::MQTT::V5::Property<Protocol::MQTT::Common::DynamicStringPair> userProp(Protocol::MQTT::V5::UserProperty, Protocol::MQTT::Common::DynamicStringPair("key", "value"));
        Protocol::MQTT::V5::ControlPacket<Protocol::MQTT::V5::CONNECT> packet;


        // Check if we have a max packet size property and if not, append one to let the server know our limitation (if any)
        packet.props.append(&maxProp); // It'll fail silently if it already exists
        packet.props.append(&userProp); // It'll fail silently if it already exists

        if (!packet.props.checkPropertiesFor(Protocol::MQTT::V5::CONNECT))
        {
            printf("Error in properties checking for CONNECT packet\n");
            return 1;
        }

        packet.fixedVariableHeader.keepAlive = 60;
        packet.fixedVariableHeader.cleanStart = 1;
        packet.fixedVariableHeader.willFlag = 0;
        packet.fixedVariableHeader.willQoS = 0;
        packet.fixedVariableHeader.willRetain = 0;
        packet.fixedVariableHeader.passwordFlag = 0;
        packet.fixedVariableHeader.usernameFlag = 0;

        // And the payload too now
        packet.payload.clientID = "clientID";

        uint32 packetSize = packet.computePacketSize(true);
        uint8 * buffer = new uint8[packetSize];
        if (!buffer)
        {
            printf("Allocation failed for CONNECT packet\n");
            return 1;
        }

        if (packet.copyInto(buffer) != packetSize)
        {
            printf("Can't serialize CONNECT packet\n");
            delete[] buffer;
            return 1;
        }

        printf("Dump of serialized packet:");
        hexDump(buffer, packetSize);

        // Then deserialize the packet now
        Protocol::MQTT::V5::ControlPacket<Protocol::MQTT::V5::CONNECT> toPacket;

        if (toPacket.readFrom(buffer, packetSize) != packetSize)
        {
            printf("Can't deserializeCONNECT packet\n");
            delete[] buffer;
            return 1;
        }
        delete[] buffer;

        // Check it's the same as input
        if (packet.fixedVariableHeader.keepAlive != toPacket.fixedVariableHeader.keepAlive) return err("Failed to match header keepAlive");
        if (packet.fixedVariableHeader.cleanStart != toPacket.fixedVariableHeader.cleanStart) return err("Failed to match header cleanStart");
        if (packet.fixedVariableHeader.willFlag != toPacket.fixedVariableHeader.willFlag) return err("Failed to match header willFlag");
        if (packet.fixedVariableHeader.willQoS != toPacket.fixedVariableHeader.willQoS) return err("Failed to match header willQoS");
        if (packet.fixedVariableHeader.willRetain != toPacket.fixedVariableHeader.willRetain) return err("Failed to match header willRetain");
        if (packet.fixedVariableHeader.passwordFlag != toPacket.fixedVariableHeader.passwordFlag) return err("Failed to match header passwordFlag");
        if (packet.fixedVariableHeader.usernameFlag != toPacket.fixedVariableHeader.usernameFlag) return err("Failed to match header usernameFlag");

        // Parse properties now
        Protocol::MQTT::V5::VisitorVariant visitor;
        while (toPacket.props.getProperty(visitor))
        {
            if (visitor.propertyType() == Protocol::MQTT::V5::PacketSizeMax) {
                auto v = visitor.as< Protocol::MQTT::V5::LittleEndianPODVisitor<uint32> >();
                if (!v) return err("Can't find property PacketSizeMax");
                if (v->getValue() != 2048) return err("Invalid PacketSizeMax property");
            }
            else if (visitor.propertyType() == Protocol::MQTT::V5::UserProperty) {
                auto v = visitor.as< Protocol::MQTT::V5::DynamicStringPairView >();
                if (!v) return err("Can't find property UserProperty");
                if (v->key != "key") return err("Invalid UserProperty key");
                if (v->value != "value") return err("Invalid UserProperty value");
            }
        }
        // All good
    }

    printf("Success\n");
    return 0;
}
