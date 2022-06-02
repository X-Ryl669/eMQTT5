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

#if MQTTUseAuth == 1
            /** An authentication packet was received. 
                @param reasonCode       Any of Success, ContinueAuthentication, ReAuthenticate
                @param authMethod       The authentication method
                @param authData         The authentication data
                @param properties       If any attached to the packet, you'll find the list here. 
                @warning By default, no action is done upon authentication packets. It's up to you to implement those packets */
            virtual void authReceived(const ReasonCodes reasonCode, const DynamicStringView & authMethod, const DynamicBinDataView & authData, const PropertiesView & properties) { }
#endif
            virtual ~MessageReceived() {}
        };
#define HasMsgRecvCB
#endif


#ifndef HasMQTTv5Client
        /** A very simple MQTTv5 client. 
            This client was made with a minimal binary size footprint in mind, yet with maximum performance.
            Currently, only the bare protocol function are supported, but that should be enough to get you started.

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
               
                    UnknownError        = -1,   //!< An unknown error happened (or the developer was too lazy to create a valid entry in this table...)
                };
                
                /** Useful operator for testing the error code */
                operator Type() const { return (Type)errorCode; }

                ErrorType(const Type type) : errorCode(type) {}
                ErrorType(const ReasonCodes code) : errorCode(code) {}
                ErrorType(const ErrorType & type) : errorCode(type.errorCode) {}
            };

            


            struct Impl;

            // Members
        private:
            /** The PImpl idiom used here to avoid exposing the internal implementation */
            Impl * impl;
            friend struct Impl;

            // Helpers
        private:
            /** Prepare, send and receive a packet */
            ErrorType::Type prepareSAR(Protocol::MQTT::V5::ControlPacketSerializable & packet, bool withAnswer = true);
            /** Enter a publish cycle. This is called upon publishing or receiving a published packet */
            ErrorType enterPublishCycle(Protocol::MQTT::V5::ControlPacketSerializableImpl & publishPacket, bool sending = false);

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
                @return An ErrorType */
            ErrorType subscribe(const char * topic, const RetainHandling retainHandling = RetainHandling::GetRetainedMessageForNewSubscriptionOnly, const bool withAutoFeedBack = false, 
                                const QoSDelivery maxAcceptedQoS = QoSDelivery::ExactlyOne, const bool retainAsPublished = true, Properties * properties = nullptr);

            /** Subscribe to some topics.

                Upon message receiving, the MessageReceived callback will be called.

                @param topics               The topics to subscribe to. This can be a filter in the form `a/b/prefix*` (prefix can be missing too)
                                            The topics represent a chained list that are successively subscribed to.
                @param properties           If provided those properties will be sent along the subscribe packet. Allowed properties for subscribe packet are: 
                                            Subscription Identifier, User property
                @return An ErrorType */
            ErrorType subscribe(SubscribeTopic & topics, Properties * properties = nullptr);


#if MQTTUseUnsubscribe == 1        
            /** Unsubscribe from some topics.

                @param topics               The topics to unsubscribe from. This can be a filter in the form `a/b/prefix*` (prefix can be missing too)
                                            @sa subscribe
                @param properties           If provided those properties will be sent along the unsubscribe packet. Allowed properties for unsubscribe packet are: 
                                            User property
                @return An ErrorType */
            ErrorType unsubscribe(UnsubscribeTopic & topics, Properties * properties = nullptr);

            /** Optional: Get the result of the last unsubscribe operation.
                Unsubscribe is handled asynchronously (unline subscribe that is expected to be called before the event loop is run).
                It's possible that a transcient publish packet arrives while the client is unsubscribing.
             
                So, typically, if you unsubscribe from a topic and want to get the result, you'll call this method until it returns an ReasonCode.
    
                @return A reason code on success or WaitingForResult if the result is not received yet */
            ErrorType getUnsubscribeResult();
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
                @return An ErrorType */
            ErrorType publish(const char * topic, const uint8 * payload, const uint32 payloadLength, const bool retain = false, const QoSDelivery QoS = QoSDelivery::AtMostOne, 
                              const uint16 packetIdentifier = 0, Properties * properties = nullptr);

            /** The client event loop you must call regularly.
                MQTT is a bidirectional protocol where the server sends packet to the client even without it asking for it.
                So you must call this method regularly to fetch any pending message and prevent the client from being disconnected from the server.

                You'll likely call this in thread/task to avoid disrupting your main application flow.
                It's safe to be called from any thread. */
            ErrorType eventLoop();

            /** Disconnect from the server
                @param code                 The disconnection reason
                @param properties           If provided those properties will be sent along the disconnect packet. Allowed properties for publish packet are: 
                                            Session Expiry Interval, Reason String, Server Reference, User property
                @return An ErrorType */
            ErrorType disconnect(const ReasonCodes code, Properties * properties = nullptr);

            /** Set the default network timeout used in millisecond */
            void setDefaultTimeout(const uint32 timeoutMs); 

            // Construction and destruction
        public:
            /** Default constructor 
                @param clientID     A client identifier if you need to provide one. If empty or null, the broker will assign one
                @param callback     A pointer to a MessageReceived callback object. The method might be called from any thread/task
                @param brokerCert   If provided, contains a view on the DER encoded broker's certificate to validate against.
                                    If provided and empty, any certificate will be accepted (not recommanded).
                                    No copy is made so please make sure the pointed data is valid while this client is valid.
                                    If you don't have a PEM encoded certificate, use this command to save the server's certificate to a .PEM file
                                    $ echo | openssl s_client -servername your.server.com -connect your.server.com:8883 2>/dev/null | openssl x509 > cert.pem                                    
                                    If you have a PEM encoded certificate, use this code to convert it to (33% smaller) DER format 
                                    $ openssl x509 -in cert.pem -outform der -out cert.der */
            MQTTv5(const char * clientID, MessageReceived * callback, const DynamicBinDataView * brokerCert = 0);
            /** Default destructor */
            ~MQTTv5();
            
        };
#define HasMQTTv5Client
#endif

    }
}


#endif
