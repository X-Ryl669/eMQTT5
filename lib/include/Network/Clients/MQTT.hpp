#ifndef hpp_CPP_MQTTClient_CPP_hpp
#define hpp_CPP_MQTTClient_CPP_hpp

// Configuration is done via macros
#include <Network/Clients/MQTTConfig.hpp>

// We need protocol declaration for this client
#include <Protocol/MQTT/MQTT.hpp>




namespace Network
{
    /** All the network client are declared here */
    namespace Client
    {
#ifndef HasMsgRecvCB
  #if MQTTQoSSupportLevel == 1
        /** Packet storage callback interface you must overload if you intend to reconnect upon network failure.
            In MQTTv5, since communication happens on a reliable transport (TCP), a QoS PUBLISH packet can be retransmitted
            ONLY IF it isn't acknowledged AND a network disconnection happens.

            This implies storing any QoS packet to be able to retransmit on reconnection.
            Since the size of the packet is unknown, this cannot be implemented without using the heap and thus, can cause memory fragmentation.

            Depending on your application, you might decide to ignore this constraint (thus, your implementation isn't standard compliant)
            or support it.

            By default, we don't reconnect automatically, and thus, never retransmit any packet nor store them.
            An implementation that does store them is provided in the test folder if you ever need it.
            Please notice that the memory requirement is 2x the size of the maximum packet * maxUnACKedPackets
            (since you can have maxUnACKedPackets outgoing QoS1 + maxUnACKedPackets outgoing QoS2) */
        struct PacketStorage
        {
            /** Save the packet buffer to be able to retransmit it on reconnection.
                @param packetID The packet identifier
                @param buffer   The packet buffer
                @param size     The size of the packet buffer in bytes
                @return true if the packet was stored, false otherwise, in which case the publishing will fail */
            virtual bool savePacketBuffer(const uint16 packetID, const uint8 * buffer, const uint32 size) { return true; }
            /** Tell the storage that a packet isn't required anymore, the storage can delete/reclaim it
                @param packetID The packet identifier
                @return true if the packet was found and deleted, false otherwise */
            virtual bool releasePacketBuffer(const uint16 packetID) { return true; }
            /** Load a packet from the packet storage to be able to retransmit it on reconnection.
                It's guaranteed that this method is called before any other packet could interfere,
                so keeping the returned pointer is safe until the packet is released.

                Since this is used to resend packet over a TCP (thus streaming) socket,
                we can skip one copy to rebuilt a contiguous packet by simply returning
                the two part in the ring buffer and let the application send them successively.
                From the receiving side, they'll be received as one contiguous packet.

                In case the storage doesn't use a ring buffer underneath, the bufferTail/sizeTail will
                always be zero.
                @param packetID     The packet identifier
                @param bufferHead   If found, the pointer will be set to the head of the packet buffer
                @param sizeHead     The size of the packet buffer head in bytes
                @param bufferTail   If found, the pointer will be set to the tail of the packet buffer. Can be null.
                @param sizeTail     The size of the packet buffer tail in bytes (or zero if no data required in tail)
                @return true if the packet was found and the arguments modified, false otherwise, in which case the publishing will abort */
            virtual bool loadPacketBuffer(const uint16 packetID, const uint8 *& bufferHead, uint32 & sizeHead, const uint8 *& bufferTail, uint32 & sizeTail) { return false; }

            virtual ~PacketStorage() {}
        };

        /** An implementation of a packet storage that stores packets in a ring buffer */
        struct RingBufferStorage : public PacketStorage
        {
            bool savePacketBuffer(const uint16 packetID, const uint8 * buffer, const uint32 size);
            bool releasePacketBuffer(const uint16 packetID);
            bool loadPacketBuffer(const uint16 packetID, const uint8 *& bufferHead, uint32 & sizeHead, const uint8 *& bufferTail, uint32 & sizeTail);

            /** The ring buffer storage size. Must be a power of 2 */
            RingBufferStorage(const size_t bufferSize, const size_t maxPacketCount);
            ~RingBufferStorage();

            struct Impl;

            // Members
        private:
            /** The PImpl idiom used here to avoid exposing the internal implementation */
            Impl * impl;
            friend struct Impl;
        };
  #else
        // Other value for QoS support level don't need to store QoS packet anyway
        typedef void PacketStorage;
  #endif
        /** Message received callback interface you must overload. */
        struct MessageReceived
        {
            typedef Protocol::MQTT::V5::DynamicStringView           DynamicStringView;
            typedef Protocol::MQTT::V5::DynamicBinDataView          DynamicBinDataView;
            typedef Protocol::MQTT::V5::PropertiesView              PropertiesView;
            typedef Protocol::MQTT::V5::ReasonCodes                 ReasonCodes;

            /** This is called upon published message reception.
                @param topic            The topic for this publication
                @param payload          The payload for this publication (can be empty)
                @param packetIdentifier If non zero, contains the packet identifier. This is usually ignored
                @param properties       If any attached to the packet, you'll find the list here. */
            virtual void messageReceived(const DynamicStringView & topic, const DynamicBinDataView & payload,
                                         const uint16 packetIdentifier, const PropertiesView & properties) = 0;
            /** This is usually called upon creation to know what it the maximum packet size you'll support.
                By default, MQTT allows up to 256MB control packets.
                On embedded system, this is very unlikely to be supported.
                This implementation does not support streaming so a buffer is created on the heap with this size
                to store the received control packet.
                @return Defaults to 2048 bytes */
            virtual uint32 maxPacketSize() const { return 2048U; }

            /** This is usually called upon connection to know how many buffers to allocate for in-flight packets.
                By default, MQTT allows any number of pending PUBLISH packet, forcing the client to store packets' ID in a ever growing buffer until they are acknowledged.
                On embedded system, this is very inconvenient since it implies heap allocating these buffers.
                Instead, MQTTv5 allows to specify how many in-flight buffers are supported by the client.
                Please notice that this number implies 3 times the number of packets ID per slot (one for QoS2 in reception
                and two for QoS1 and QoS2 in transmission) (that's 12 bytes per slot).
                @return Defaults to 1 */
            virtual uint32 maxUnACKedPackets() const { return 1U; }

            /** This is called upon disconnection.
                If any method detect a disconnection or a unexpected and fatal socket error, this will be called.
                A standard client will likely reconnect here, and will resend any pending QoS packets.
                The default implementation doesn't do anything since this client doesn't store the credentials used for connection.
                So it won't reconnect by itself. However, if you're reconnecting without destructing the client, the first call to the event loop will resend unACKed packets.
                @note This is not compliant to continue using the client as if nothing happened after this function is called.
                @param reasonCode       The reason for the disconnection if known */
            virtual void connectionLost(const ReasonCodes reasonCode) {}

#if MQTTUseAuth == 1
            /** An authentication packet was received.
                This is called either during connection and in the event loop in case the server started it
                @param reasonCode       Any of Success, ContinueAuthentication, ReAuthenticate or NotAuthorized, BadAuthenticationMethod
                @param authMethod       The authentication method
                @param authData         The authentication data
                @param properties       If any attached to the packet, you'll find the list here.
                @return true            If authentication was a success or false otherwise
                @warning By default, no action is done upon authentication packets. It's up to you to implement those packets */
            virtual bool authReceived(const ReasonCodes reasonCode, const DynamicStringView & authMethod, const DynamicBinDataView & authData, const PropertiesView & properties)
            {
                return false;
            }
#endif
            virtual ~MessageReceived() {}
        };
#define HasMsgRecvCB
#endif


#ifndef HasMQTTv5Client
        /** A very simple MQTTv5 client.
            This client was made with a minimal binary size footprint in mind, yet with maximum performance.
            It can be configured to only implement bare protocol functions (that's enough for usual usage pattern)
            or a more complete featureset (you choose what method to add depending on your need).

            Typically, you can add Quality Of Service feature, dynamic unsubscribing, and advanced authentication support.

            Concerning multithreading and reentrancy support, this client is fully reentrant.
            This means that you can call `publish` in the `messageReceived` callback, or `auth` in the `authReceived` callback.
            The client is not protected by a global mutex (unlike in version 1.0) but its internal implementation is
            protected against multithreaded access.

            The expected usage pattern is to call `connectTo`, `auth`, `subscribe`, `unsubscribe` and `disconnect`
            from the same thread that's calling `eventLoop`.
            You can call `publish` from any thread.
            In case of error, `publish` will return the error code but will not destruct the socket
            Socket destruction will only happen in the next call to `eventLoop` and it's protected so it can't happen
            while a `publish` operation is running.
            When `eventLoop` returns with an error code, you can safely call `connectTo` to resume and restart the communication.

            Notice however that you must not delete the client (if constructed on heap) while accessing from multiple thread.


            @warning By default, to minimize allocations, properties provided to the methods below are captured (swapped with an empty set).
                     If you want to avoid this, you'll need to clone them beforehand.
        */
        struct MQTTv5
        {
            // Type definition and enumeration
        public:
            /** Just because typing this repeatedly is painful */
            typedef Protocol::MQTT::V5::DynamicString               DynamicString;
            typedef Protocol::MQTT::V5::DynamicStringView           DynamicStringView;
            typedef Protocol::MQTT::V5::DynamicBinaryData           DynamicBinaryData;
            typedef Protocol::MQTT::V5::DynamicBinDataView          DynamicBinDataView;
            typedef Protocol::MQTT::V5::Properties                  Properties;
            typedef Protocol::MQTT::V5::PropertiesView              PropertiesView;
            typedef Protocol::MQTT::V5::WillMessage                 WillMessage;
            typedef Protocol::MQTT::V5::QualityOfServiceDelivery    QoSDelivery;
            typedef Protocol::MQTT::V5::RetainHandling              RetainHandling;
            typedef Protocol::MQTT::V5::ReasonCodes                 ReasonCodes;
            typedef Protocol::MQTT::V5::SubscribeTopic              SubscribeTopic;
            typedef Protocol::MQTT::V5::UnsubscribeTopic            UnsubscribeTopic;

            /** The error type returned by all methods */
            struct ErrorType
            {
                /** The error code for this error type */
                int32  errorCode;

                /** Between 0x4 and 0xA2, the error code reflect a reason codes (@sa Protocol::MQTT::V5::ReasonCodes) */
                enum Type
                {
                    Success             = 0,    //!< The method succeeded as expected
                    TimedOut            = -2,   //!< The operation timed out
                    AlreadyConnected    = -3,   //!< Already connected to a server
                    BadParameter        = -4,   //!< Bad parameter for this method call
                    BadProperties       = -5,   //!< The given properties are unexpected (no packet was sent yet)
                    NetworkError        = -6,   //!< A communication with the network failed
                    NotConnected        = -7,   //!< Not connected to the server
                    TranscientPacket    = -8,   //!< A transcient packet was captured and need to be processed first
                    WaitingForResult    = -9,   //!< The available answer is not ready yet, need to call again later on
                    StorageError        = -10,  //!< Can't store the value as expected

                    UnknownError        = -1,   //!< An unknown error happened (or the developer was too lazy to create a valid entry in this table...)
                };

                /** Useful operator for testing the error code */
                operator Type() const { return (Type)errorCode; }

                ErrorType(const Type type) : errorCode(type) {}
                ErrorType(const ReasonCodes code) : errorCode(code) {}
            };




            struct Impl;

            // Members
        private:
            /** The PImpl idiom used here to avoid exposing the internal implementation */
            Impl * impl;
            friend struct Impl;

            // Interface
        public:
            /** Connect to the given server URL.
                Unlike the other connect to method, this method parses the URL to figure out the address and port.
                Please notice that URL parsing code adds some bloat to the binary, so if you already know the
                address and port beforehand, use the other one so this method can be removed from the binary.

                @param serverHost           The server host name (without any scheme or port). This should be like the DNS name for the server or a IP address
                @param port                 The server port value
                @param useTLS               Should the connection happen over TLS
                @param keepAliveTimeInSec   The keep alive delay in seconds (this is an hint, the server can force its own)
                @param cleanStart           If true, both the server and client will discard any previous session, else the server will try to reuse any previous live session
                @param userName             If provided, this username is used for authentication against the server
                @param password             If provided, this password is used for authentication against the server
                @param willMessage          If provided, this will message will be used when client is disconnected abruptly
                @param willQoS              If provided and a willMessage is set, the will message will be published with this quality of service level (from 0 to 2)
                @param willRetain           If provided and a willMessage is set, the will message will be retained for the topic for future subscriptions on this topic
                @param properties           If provided those properties will be sent along the connect packet. Allowed properties for connect packet are:
                                            Session expiry interval, Receive Maximum, Maximum Packet Size, Topic Alias Maximum,
                                            Request Response Information, Request Problem Information, User Property, Authentication Method, Authentication Data

                @note This is expected to be called before the eventLoop thread is started or in the eventLoop thread.
                @return An ErrorType */
            ErrorType connectTo(const char * serverHost, const uint16 port, bool useTLS = false, const uint16 keepAliveTimeInSec = 300,
                const bool cleanStart = true, const char * userName = nullptr, const DynamicBinDataView * password = nullptr,
                WillMessage * willMessage = nullptr, const QoSDelivery willQoS = QoSDelivery::AtMostOne, const bool willRetain = false,
                Properties * properties = nullptr);

#if MQTTUseAuth == 1
            /** Authenticate with the given server.
                This must be called after connectTo succeeded with the auth callback has been called
                @param reasonCode           Any of Success, ContinueAuthentication, ReAuthenticate
                @param authMethod           The authentication method
                @param authData             The authentication data
                @param properties           If provided those properties will be sent along the auth packet. Allowed properties for connect packet are:
                                            User Property, Reason string, Authentication Method, Authentication Data

                @note This is expected to be called before the eventLoop thread is started or in the eventLoop thread. It's safe to call in the authReceived callback.
                @return An ErrorType */
            ErrorType auth(const ReasonCodes reasonCode, const DynamicStringView & authMethod, const DynamicBinDataView & authData, Properties * properties = nullptr);
#endif

            /** Subscribe to a topic.
                You can subscripe to a single topic or a topic list using the `*` wildcard.
                Please notice that this client does not create multiple topic per packet.
                Call this method multiple times instead.

                Upon message receiving, the MessageReceived callback will be called.

                @param topic                The topic to subscribe to. This can be a filter in the form `a/b/prefix*` (prefix can be missing too)
                @param maxAcceptedQoS       The maximum accepted quality of service. The client can deal with any level, so you can safely ignore this.
                @param retainAsPublished    If true, the Retain flag from the publish message will be kept in the packet send upon subscription (this is typically used for proxy brokers)
                @param retainHandling       The retain handling policy (typically, how you want to receive a retained message for the topic)
                @param withAutoFeedback     If true, if you publish on this topic, you'll receive your message as well
                @param properties           If provided those properties will be sent along the subscribe packet. Allowed properties for subscribe packet are:
                                            Subscription Identifier, User property

                @note This is expected to be called before the eventLoop thread is started or in the eventLoop thread.
                @return An ErrorType */
            ErrorType subscribe(const char * topic, const RetainHandling retainHandling = RetainHandling::GetRetainedMessageForNewSubscriptionOnly, const bool withAutoFeedBack = false,
                                const QoSDelivery maxAcceptedQoS = QoSDelivery::ExactlyOne, const bool retainAsPublished = true, Properties * properties = nullptr);

            /** Subscribe to some topics.

                Upon message receiving, the MessageReceived callback will be called.

                @param topics               The topics to subscribe to. This can be a filter in the form `a/b/prefix*` (prefix can be missing too)
                                            The topics represent a chained list that are successively subscribed to.
                @param properties           If provided those properties will be sent along the subscribe packet. Allowed properties for subscribe packet are:
                                            Subscription Identifier, User property

                @note This is expected to be called before the eventLoop thread is started or in the eventLoop thread.
                @return An ErrorType */
            ErrorType subscribe(SubscribeTopic & topics, Properties * properties = nullptr);


#if MQTTUseUnsubscribe == 1
            /** Unsubscribe from some topics.

                @param topics               The topics to unsubscribe from. This can be a filter in the form `a/b/prefix*` (prefix can be missing too)
                                            @sa subscribe
                @param properties           If provided those properties will be sent along the unsubscribe packet. Allowed properties for unsubscribe packet are:
                                            User property

                @note This is expected to be called before the eventLoop thread is started or in the eventLoop thread.
                @return A reason code or success */
            ErrorType unsubscribe(UnsubscribeTopic & topics, Properties * properties = nullptr);
#endif

            /** Publish to a topic.
                @param topic                The topic to publish into.
                @param payload              The payload to send to this publication, can be null
                @param payloadLength        The length of the payload in bytes
                @param retain               The retain flag for this message. If true, this message will stick to the topic and will (usually) be send to new subscribers.
                @param QoS                  The quality of service delivery flag to use.
                @param packetIdentifier     If using a QoS different than AtMostOne, you can force packet identifier (leave to 0 for auto selection of this identifier)
                @param properties           If provided those properties will be sent along the publish packet. Allowed properties for publish packet are:
                                            Payload Format Indicator, Message Expiry Interval, Topic Alias,
                                            Response topic, Correlation Data, Subscription Identifier, User property, Content Type
                @return An ErrorType
                @note You can call this method anytime from anywhere (including from inside a messageReceived callback) and in a different thread.
                      Upon an error return, the socket isn't closed automatically (since another thread might be publishing at the same time).
                      The next call to eventLoop() in its thread will clear the socket, call the connectionLost() callback and that's where you'll be able to
                      reconnect with connectTo()  */
            ErrorType publish(const char * topic, const uint8 * payload, const uint32 payloadLength, const bool retain = false, const QoSDelivery QoS = QoSDelivery::AtMostOne,
                              const uint16 packetIdentifier = 0, Properties * properties = nullptr);

            /** The client event loop you must call regularly.
                MQTT is a bidirectional protocol where the server sends packet to the client even without it asking for it.
                So you must call this method regularly to fetch any pending message and prevent the client from being disconnected from the server.

                You'll likely call this in thread/task to avoid disrupting your main application flow.
                It's safe to be called from any thread. This method will likely call MessageReceived::messageReceived callback upon receiving a message.

                @warning Don't call eventLoop from your MessageReceived::messageReceived callback to avoid recursion. */
            ErrorType eventLoop();

            /** Disconnect from the server
                @param code                 The disconnection reason
                @param properties           If provided those properties will be sent along the disconnect packet. Allowed properties for publish packet are:
                                            Session Expiry Interval, Reason String, Server Reference, User property

                @note This is expected to be called before the eventLoop thread is started or in the eventLoop thread.
                @return An ErrorType */
            ErrorType disconnect(const ReasonCodes code, Properties * properties = nullptr);

            /** Set the default network timeout used in millisecond */
            void setDefaultTimeout(const uint32 timeoutMs);

            // Construction and destruction
        public:
            /** Default constructor
                @param clientID     A client identifier if you need to provide one. If empty or null, the broker will assign one
                @param callback     A pointer to a MessageReceived callback object. The method might be called from any thread/task
                @param storage      A pointer to a PacketStorage implementation (used for QoS retransmission) that's owned.
                                    If null a default one will be used that stores packet in a ring buffer (allocating memory for it).
                                    You can use "new PacketStorage()" here to skip any memory allocation but the client won't be 100% compliant here,
                                    it'll just fail to retransmit any QoS packet after resuming from a connection loss.
                @param brokerCert   If provided, contains a view on the DER encoded broker's certificate to validate against.
                                    If provided and empty, any certificate will be accepted (not recommanded).
                                    No copy is made so please make sure the pointed data is valid while this client is valid.
                                    If you don't have a PEM encoded certificate, use this command to save the server's certificate to a .PEM file
                                    $ echo | openssl s_client -servername your.server.com -connect your.server.com:8883 2>/dev/null | openssl x509 > cert.pem
                                    If you have a PEM encoded certificate, use this code to convert it to (33% smaller) DER format
                                    $ openssl x509 -in cert.pem -outform der -out cert.der
                @param clientCert   If provided, contains a view on the DER encoded client's certificate to provide on connection.
                                    Required for two-way / mutual TLS.
                @param clientKey    If provided, contains a view on the client's private key
                                    Required for two-way / mutual TLS. */

            MQTTv5(const char * clientID, MessageReceived * callback, PacketStorage * storage = 0, const DynamicBinDataView * brokerCert = 0,
                   const DynamicBinDataView * clientCert = 0, const DynamicBinDataView * clientKey = 0);
            /** Default destructor */
            ~MQTTv5();
            /** Set the client ID explicitely.
                This is used for reconnecting to a existing client session
                @param clientID     A client identifier if you need to provide one. If empty or null, the broker will assign one on next connectTo call */
            void setClientID(const char * clientID);

        };
#define HasMQTTv5Client
#endif

    }
}


#endif
