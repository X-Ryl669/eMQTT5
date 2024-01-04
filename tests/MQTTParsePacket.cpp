// Usual programs
#include <stdio.h>
#include <stdlib.h>

// We need Logger
#include "Logger/Logger.hpp"
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

struct InitLogger {
    InitLogger(bool withDump) { const unsigned int logMask = ::Logger::Creation|::Logger::Error|::Logger::Network|::Logger::Connection|::Logger::Content|::Logger::Deletion|(withDump ? ::Logger::Dump : 0);
    ::Logger::setDefaultSink(new ::Logger::DebugConsoleSink(logMask)); }
};

inline int asHex(char ch) { return ch >= '0' && ch <= '9' ? (ch - '0') : (ch >= 'a' && ch <= 'f' ? (ch - 'a' + 10) : (ch >= 'A' && ch <= 'F' ? (ch - 'A' + 10) : 0)); }

struct ScopeFile
{
    FILE * f;
    operator FILE *() const { return f; }
    ScopeFile(const char * path) : f(fopen(path, "rb")) {}
    ~ScopeFile() { if (f) fclose(f); }
};

void hexDump(const uint8 * inBuffer, size_t inSize)
{
    for (size_t i = 0; i < inSize; i++)
    {
        if (!(i%16)) printf("\n%08X ", i);
        printf("%02X ", inBuffer[i]);
    }
}

bool readFile(const char * path, uint8 *& buffer, size_t & size)
{
    ScopeFile f(path);
    if (!f) return false;
    if (fseek(f, 0,  SEEK_END)) return false;
    size = (size_t)ftell(f);
    if (fseek(f, 0, SEEK_SET)) return false;

    if (!size || size > 2048*1024) return false;
    buffer = new uint8[size+1];
    if (!buffer) return false;

    int r = fread(buffer, 1, size, f);
    return r == size;
}

int main(int argc, char ** argv)
{
    InitLogger initLogger(true);
    // First convert the input from what it is to something we can parse
    if (argc == 1 || (argc == 2 && String("--help") == argv[1]))
    {
        printf("MQTTv5 Packet Parser\nUsage is: %s 0x34 0xC3  or %s 12 23 45 AB CE or %s \"12ACBEC345353\" or %s -f fileToParse\n", argv[0], argv[0], argv[0], argv[0]);
        return 0;
    }

    uint8 * inBuffer = 0;
    size_t inSize = 0;
    if (argc == 3 && String("-f") == argv[1])
    {
        if (!readFile(argv[2], inBuffer, inSize))
            return fprintf(stderr, "Can't read the given file: %s\n", argv[2]);
    }
    else
    {
        // Concatenate input
        String data;
        for (int i = 1; i < argc; i++) data += argv[i];
        String input = data.replacedAll("\"", "").replacedAll("0x", "").replacedAll(" ", "").replacedAll(",", "");
        // Then convert to a memory block
        inBuffer = new uint8[input.getLength() / 2 + 1];
        for (int i = 0; i < input.getLength(); i+=2)
        {
           char hi = input[i], lo = input[i+1]; // This works even if out of buffer since default value is zero in that case;
           inBuffer[inSize++] = (asHex(hi) << 4) | asHex(lo);
        }
    }
    // Then try to parse it
    if (inSize < 2) return fprintf(stderr, "Bad packet size, minimum 2 bytes required\n");
    Protocol::MQTT::V5::FixedHeader header;
    header.raw = inBuffer[0];
    printf("Detected %s packet\n", Protocol::MQTT::V5::Helper::getControlPacketName((Protocol::MQTT::Common::ControlPacketType)(uint8)header.type));
    Protocol::MQTT::Common::VBInt len;
    uint32 r = len.readFrom(&inBuffer[1], inSize - 1);
    if (r == Protocol::MQTT::Common::BadData)
        return fprintf(stderr, "Invalid packet length at pos: 1\n"); // Close the socket here, the given data are wrong or not the right protocol
    if (r == Protocol::MQTT::Common::NotEnoughData)
        return fprintf(stderr, "Packet is too short at pos: 1\n");
    // Check packet size
    if ((uint32)len + 1 + len.getSize() < inSize)
        printf("Warning: Got additional %d bytes but packet size is coded as: %u\n", (int)inSize, (uint32)len + 1 + len.getSize());
    else printf("with size: %u\n", (uint32)len + 1 + len.getSize());

    // Then dump it now
    Protocol::MQTT::V5::registerAllProperties();
    Protocol::MQTT::V5::ControlPacketSerializable * packet;
    switch ((uint8)header.type)
    {
    case Protocol::MQTT::V5::RESERVED: return fprintf(stderr, "Can not parse further...\n");
    case Protocol::MQTT::V5::CONNECT:     packet = new Protocol::MQTT::V5::ConnectPacket; break;
    case Protocol::MQTT::V5::CONNACK:     packet = new Protocol::MQTT::V5::ROConnACKPacket; break;
    case Protocol::MQTT::V5::PUBLISH:     packet = new Protocol::MQTT::V5::ROPublishPacket; break;
    case Protocol::MQTT::V5::PUBACK:      packet = new Protocol::MQTT::V5::ROPubACKPacket; break;
    case Protocol::MQTT::V5::PUBREC:      packet = new Protocol::MQTT::V5::ROPubRecPacket; break;
    case Protocol::MQTT::V5::PUBREL:      packet = new Protocol::MQTT::V5::ROPubRelPacket; break;
    case Protocol::MQTT::V5::PUBCOMP:     packet = new Protocol::MQTT::V5::ROPubCompPacket; break;
    case Protocol::MQTT::V5::SUBSCRIBE:   packet = new Protocol::MQTT::V5::ControlPacket<Protocol::MQTT::Common::SUBSCRIBE>; break;
    case Protocol::MQTT::V5::SUBACK:      packet = new Protocol::MQTT::V5::ROSubACKPacket; break;
    case Protocol::MQTT::V5::UNSUBSCRIBE: packet = new Protocol::MQTT::V5::ControlPacket<Protocol::MQTT::Common::UNSUBSCRIBE>; break;
    case Protocol::MQTT::V5::UNSUBACK:    packet = new Protocol::MQTT::V5::ROUnsubACKPacket; break;
    case Protocol::MQTT::V5::PINGREQ:     packet = new Protocol::MQTT::V5::PingReqPacket; break;
    case Protocol::MQTT::V5::PINGRESP:    packet = new Protocol::MQTT::V5::PingRespPacket; break;
    case Protocol::MQTT::V5::DISCONNECT:  packet = new Protocol::MQTT::V5::RODisconnectPacket; break;
    case Protocol::MQTT::V5::AUTH:        packet = new Protocol::MQTT::V5::ROAuthPacket; break;
    }
    r = packet->readFrom(inBuffer, inSize);
    if (Protocol::MQTT::Common::isError(r))
        return fprintf(stderr, "Could not parse packet with error: %u\n", r);
    // Then dump the packet
    String out;
//    if (!
packet->dump(out);
//        return fprintf(stderr, "Could not dump the packet\n");
    printf("%s\n", (const char*)out);

    // Adding packet hexdump too is useful for debugging
    printf("\nFrom input buffer:");
    hexDump(inBuffer, inSize);
    printf("\n");
    return 0;
}
