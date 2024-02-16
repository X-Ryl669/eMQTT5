// Usual programs
#include <stdio.h>
#include <stdlib.h>

// We need MQTT client
#include "Network/Clients/MQTT.hpp"
// We need URL parsing too
#include "Network/Address.hpp"
// We need command line parsing too to avoid NIH
#include "Platform/Arguments.hpp"
#include "Logger/Logger.hpp"


typedef Strings::FastString String;

struct InitLogger {
    InitLogger(bool withDump) { const unsigned int logMask = ::Logger::Creation|::Logger::Error|::Logger::Network|::Logger::Connection|::Logger::Content|::Logger::Deletion|(withDump ? ::Logger::Dump : 0);
    ::Logger::setDefaultSink(new ::Logger::DebugConsoleSink(logMask)); }
};

struct MessageReceiver : public Network::Client::MessageReceived
{
    void messageReceived(const Network::Client::MQTTv5::DynamicStringView & topic, const Network::Client::MQTTv5::DynamicBinDataView & payload,
                         const uint16 packetIdentifier, const Network::Client::MQTTv5::PropertiesView & properties)
    {
        fprintf(stdout, "Msg received: (%04X)\n", packetIdentifier);
        fprintf(stdout, "  Topic: %.*s\n", topic.length, topic.data);
        fprintf(stdout, "  Payload: %.*s\n", payload.length, payload.data);
    }
#if MQTTUseAuth == 1
    bool authReceived(const ReasonCodes reasonCode, const DynamicStringView & authMethod, const DynamicBinDataView & authData, const PropertiesView & properties)
    {
        fprintf(stdout, "Auth packet received\n");
        fprintf(stdout, "  AuthMethod: %.*s\n", authMethod.length, authMethod.data);
        fprintf(stdout, "  AuthData: %.*s\n", authData.length, authData.data);
        fprintf(stdout, "  Reason Code: %d\n", (int)reasonCode);

        if (authData.length != strlen("Whizz") || memcmp(authData.data, "Whizz", authData.length))
        {
            fprintf(stdout, "Bad authentication answer from server");
            return false;
        }
        DynamicBinDataView data(strlen("Bees"), (const uint8*)"Bees");
        if (Network::Client::MQTTv5::ErrorType ret = client->auth(Protocol::MQTT::V5::ContinueAuthentication, authMethod, data))
        {
            fprintf(stdout, "Failed auth with error: %d\n", (int)ret);
            return false;
        }
        return true;
    }
    Network::Client::MQTTv5 * client;
#endif
};

String publishTopic, publishMessage;
String publish(const char * topic, const char * message)
{
    // Remember the message to publish that we'll do once connected
    publishTopic = topic; publishMessage = message;
    return "";
}

Network::Client::MQTTv5::QoSDelivery QoS = Network::Client::MQTTv5::QoSDelivery::AtMostOne;
String setQoS(const String & qos)
{
    if (qos == "0" || qos.caselessEqual("atmostone")) QoS = Network::Client::MQTTv5::QoSDelivery::AtMostOne;
    else if (qos == "1" || qos.caselessEqual("atleastone")) QoS = Network::Client::MQTTv5::QoSDelivery::AtLeastOne;
    else if (qos == "2" || qos.caselessEqual("exactlyone")) QoS = Network::Client::MQTTv5::QoSDelivery::ExactlyOne;
    else
    {
        return "Please specify either 0 or atleastone, 1 or atmostone, 2 or exactlyone for QoS option";
    }
    return "";
}

volatile bool cont = true;
void ctrlc(int sig)
{
    if (sig == SIGINT) cont = false;
}

#if MQTTUseTLS == 1
struct ScopeFile
{
    FILE * f;
    operator FILE *() const { return f; }
    ScopeFile(const char * path) : f(fopen(path, "rb")) {}
    ~ScopeFile() { if (f) fclose(f); }
};

String readFile(const String & path)
{
    String ret;
    ScopeFile f(path);
    if (!f) return ret;
    if (fseek(f, 0,  SEEK_END)) return ret;
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET)) return ret;

    if (!size || size > 2048*1024) return ret;
    int r = fread(ret.Alloc(size), 1, size, f);
    ret.releaseLock(r);
    return ret;
}
#endif

int main(int argc, const char ** argv)
{

    String server;
    String username;
    String password;
    String clientID;
    String subscribe;
    String certFile;
    unsigned keepAlive = 300;
    bool   dumpComm = false;
    bool   retainPublishedMessage = false;


    Arguments::declare(server, "The server URL (for example 'mqtt.mine.com:1883')", "server");
    Arguments::declare(username, "The username to use", "username");
    Arguments::declare(password, "The password to use", "password", "pw");
    Arguments::declare(clientID, "The client identifier to use", "clientid");
    Arguments::declare(keepAlive, "The client keep alive time", "keepalive");
    Arguments::declare(publish, "Publish on the topic the given message", "publish", "pub");
    Arguments::declare(retainPublishedMessage, "Retain published message", "retain");
    Arguments::declare(setQoS, "Quality of service for publishing or subscribing", "qos");
    Arguments::declare(subscribe, "The subscription topic", "subscribe", "sub");
    Arguments::declare(certFile, "Expected broker certificate in DER format", "der");

    Arguments::declare(dumpComm, "Dump communication", "verbose");

    String error = Arguments::parse(argc, argv);
    if (error)
    {
        fprintf(stderr, "%s\n", (const char*)error);
        return argc != 1;
    }
    if (!server) return fprintf(stderr, "No server URL given. Leaving...\n");

    InitLogger initLogger(dumpComm);


    // Ok, parse the given URL
    // Add a scheme if none provided
    if (!server.fromFirst("://")) server = "mqtt://" + server;
    Network::Address::URL serverURL(server);
    uint16 port = serverURL.stripPortFromAuthority(1883);
    MessageReceiver receiver;

#if MQTTUseTLS == 1
    Protocol::MQTT::Common::DynamicBinaryData brokerCert;
    if (certFile)
    {
        // Load the certificate if provided
        String certContent = readFile(certFile);
        brokerCert = Protocol::MQTT::Common::DynamicBinaryData(certContent.getLength(), (const uint8*)certContent);
    }
    Protocol::MQTT::Common::DynamicBinDataView certView(brokerCert);
    Network::Client::MQTTv5 client(clientID, &receiver, certFile ? &certView : (Network::Client::MQTTv5::DynamicBinDataView*)0);
#else
    Network::Client::MQTTv5 client(clientID, &receiver);
#endif
    Network::Client::MQTTv5::DynamicBinDataView pw(password.getLength(), (const uint8*)password);

#if MQTTUseAuth == 1
    receiver.client = &client;
    Protocol::MQTT::V5::Property<Network::Client::MQTTv5::DynamicStringView> method(Protocol::MQTT::V5::AuthenticationMethod, Network::Client::MQTTv5::DynamicStringView("DumbledoreOffice"));
    Protocol::MQTT::V5::Property<Network::Client::MQTTv5::DynamicBinDataView> data(Protocol::MQTT::V5::AuthenticationData, Network::Client::MQTTv5::DynamicBinDataView(strlen("Fizz"), (const uint8*)"Fizz"));
    Protocol::MQTT::V5::Properties props;

    props.append(&method);
    props.append(&data);

    if (Network::Client::MQTTv5::ErrorType ret = client.connectTo(serverURL.getAuthority(), port, serverURL.getScheme().caselessEqual("mqtts"),
                                                                  (uint16)min(65535U, keepAlive), true, username ? (const char*)username : nullptr, password ? &pw : nullptr, nullptr, QoS, false, &props))
#else
    if (Network::Client::MQTTv5::ErrorType ret = client.connectTo(serverURL.getAuthority(), port, serverURL.getScheme().caselessEqual("mqtts"),
                                                                  (uint16)min(65535U, keepAlive), true, username ? (const char*)username : nullptr, password ? &pw : nullptr))
#endif
    {
        return fprintf(stderr, "Failed connection to %s with error: %d\n", (const char*)serverURL.asText(), (int)ret);
    }
    printf("Connected to %s\n", (const char*)serverURL.asText());

    // Check if we have some subscription
    if (subscribe)
    {
        if (Network::Client::MQTTv5::ErrorType ret = client.subscribe(subscribe, Protocol::MQTT::V5::GetRetainedMessageAtSubscriptionTime, true, QoS, retainPublishedMessage))
        {
            return fprintf(stderr, "Failed subscribing to %s with error: %d\n", (const char*)subscribe, (int)ret);
        }
        printf("Subscribed to %s\nWaiting for messages...\n", (const char*)subscribe);

        // Then enter the event loop here
        signal(SIGINT, ctrlc);
        while (cont)
        {
            if (Network::Client::MQTTv5::ErrorType ret = client.eventLoop())
                return fprintf(stderr, "Event loop failed with error: %d\n", (int)ret);

        }

#if MQTTUseUnsubscribe == 1
        // Unsubscribe from the topic here
        Protocol::MQTT::V5::UnsubscribeTopic topic((const char*)subscribe, true);
        if (Network::Client::MQTTv5::ErrorType ret = client.unsubscribe(topic, 0))
        {
            return fprintf(stderr, "Failed unsubscribing to %s with error: %d\n", (const char*)subscribe, (int)ret);
        }
        // Run the event loop once more to fetch the unsubscribe ACK (not absolutely required when leaving, but for sample code
        if (Network::Client::MQTTv5::ErrorType ret = client.eventLoop())
            return fprintf(stderr, "Event loop failed with error: %d\n", (int)ret);

        Network::Client::MQTTv5::ErrorType ret = client.getUnsubscribeResult();
        fprintf(ret == 0 ? stdout : stderr, "Unsubscribe result: %d\n", (int)ret);
#endif

        return 0;
    }

    // Check if we have something to publish
    if (publishTopic)
    {
        // Publish
        if (Network::Client::MQTTv5::ErrorType ret = client.publish(publishTopic, publishMessage, publishMessage.getLength(), retainPublishedMessage, QoS))
        {
            return fprintf(stderr, "Failed publishing %s to %s with error: %d\n", (const char*)publishMessage, (const char*)publishTopic, (int)ret);
        }
        printf("Published %s to %s\n", (const char*)publishMessage, (const char*)publishTopic);
        return 0;
    }

    return 0;
}
