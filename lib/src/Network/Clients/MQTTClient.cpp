// We need our implementation
#include <Network/Clients/MQTT.hpp>

#if MQTTUseAuth == 1
  /** Used to track reentrancy in the AUTH recursive scheme */
  enum AuthReentrancy
  {
     FromConnect = 0x80000000,
     AuthMask    = 0x7FFFFFFF,
  };
#endif


#if MQTTOnlyBSDSocket != 1
#pragma message("This configuration is not supported and depends on external code in tests folder that is not exported upon install")
// We need socket declaration
#include "Network/Socket.hpp"
// We need SSL socket declaration too
//#include "../../../include/Network/SSLSocket.hpp"
// We need FastLock too
#include "Threading/Lock.hpp"
#else
// We need BSD socket here
#include <sys/socket.h>
// We need gethostbyname or getaddrinfo
#include <netdb.h>
// We need TCP_NODELAY
#include <netinet/tcp.h>
// We need std::atomic too
#include <atomic>
  #if MQTTUseTLS == 1
    // We need MBedTLS code
    #include <mbedtls/certs.h>
    #include <mbedtls/ctr_drbg.h>
    #include <mbedtls/entropy.h>
    #include <mbedtls/error.h>
    #include <mbedtls/net_sockets.h>
    #include <mbedtls/platform.h>
    #include <mbedtls/ssl.h>
  #endif
#endif
// We need StackHeapBuffer to avoid stressing the heap allocator when it's not required
#include <Platform/StackHeapBuffer.hpp>

#if MQTTDumpCommunication == 1
  // We need hexDump
  #include "Utils/Dump.hpp"
#endif

// This is the maximum allocation that'll be performed on the stack before it's being replaced by heap allocation
// This also means that the stack size for the thread using such function must be larger than this value
#define StackSizeAllocationLimit    512


namespace Network { namespace Client {

#if MQTTOnlyBSDSocket != 1
    /*  The socket class we are using for socket operations.
        There's a default implementation for Berkeley socket and (Open)SSL socket in the ClassPath, but
        you can implement any library you want, like, for example, lwIP, so change this if you do */
    typedef Network::Socket::BerkeleySocket Socket;
  #if MQTTUseTLS == 1
    /*  The SSL socket we are using (when using SSL/TLS connection).
        There's a default implementation for (Open/Libre)SSL socket in ClassPath, but you can implement
        one class with, for example, MBEDTLS here if you want. Change this if you do */
    typedef Network::Socket::SSL_TLS        SSLSocket;
    /** The SSL context to (re)use. If you need to skip negotiating, you'll need to modify this context */
    typedef SSLSocket::SSLContext           SSLContext;
  #endif

    /** The scoped lock class we are using */
    typedef Threading::ScopedLock   ScopedLock;

    struct MQTTv5::Impl
    {
        /** The multithread protection for this object */
        Threading::Lock                 lock;
        /** This client socket */
        Socket *                        socket;
        /** The DER encoded certificate (if provided) */
        const DynamicBinDataView  *     brokerCert;
  #if MQTTUseTLS == 1
        /** The SSL context (if any used) */
        SSLContext *                    sslContext;
  #endif
        /** This client unique identifier */
        DynamicString                   clientID;
        /** The message received callback to use */
        MessageReceived *               cb;
        /** The default timeout in milliseconds */
        uint32                          timeoutMs;

        /** The last communication time in second */
        uint32                          lastCommunication;
        /** The publish current default identifier allocator */
        uint16                          publishCurrentId;
        /** The keep alive delay in seconds */
        uint16                          keepAlive;


        /** The reading state. Because data on a TCP stream is
            a stream, we have to remember what state we are currently following while parsing data */
        enum RecvState
        {
            Ready = 0,
            GotType,
            GotLength,
            GotCompletePacket,
        }                   recvState;
        /** The receiving buffer size */
        uint32              recvBufferSize;
        /** The maximum packet size the server is willing to accept */
        uint32              maxPacketSize;
        /** The available data in the buffer */
        uint32              available;
        /** The receiving data buffer */
        uint8   *           recvBuffer;
        /** The receiving VBInt size for the packet header */
        uint8               packetExpectedVBSize;

  #if MQTTUseAuth == 1
        /** Used to track the origin of the AUTH exchange */
        uint32                authSource;
  #endif


        uint16 allocatePacketID()
        {
            return ++publishCurrentId;
        }

        Impl(const char * clientID, MessageReceived * callback, const DynamicBinDataView * brokerCert)
             : socket(0), brokerCert(brokerCert),
  #if MQTTUseTLS == 1
               sslContext(0),
  #endif
  #if MQTTUseAuth == 1
               authSource(0),
  #endif
               clientID(clientID), cb(callback), timeoutMs(3000), lastCommunication(0), publishCurrentId(0), keepAlive(300),
               recvState(Ready), recvBufferSize(max(callback->maxPacketSize(), 8U)), maxPacketSize(65535), available(0), recvBuffer((uint8*)::malloc(recvBufferSize)), packetExpectedVBSize(Protocol::MQTT::Common::VBInt(recvBufferSize).getSize())
        {}
        ~Impl() { delete socket; socket = 0; ::free(recvBuffer); recvBuffer = 0; recvBufferSize = 0; }


        inline void setTimeout(uint32 timeout) { timeoutMs = timeout; }

        bool shouldPing()
        {
            return (((uint32)time(NULL) - lastCommunication) >= keepAlive);
        }

        int send(const char * buffer, const uint32 length)
        {
            if (!socket) return -1;
#if MQTTDumpCommunication == 1
            // Dump the packet to send
            String packetDump;
            Utils::hexDump(packetDump, (const uint8*)buffer, length, 16, true, true);
            Protocol::MQTT::V5::FixedHeader header;
            header.raw = buffer[0];
            Logger::log(Logger::Dump, "> Sending packet: %s(R:%d,Q:%d,D:%d)%s", Protocol::MQTT::V5::Helper::getControlPacketName((Protocol::MQTT::Common::ControlPacketType)(uint8)header.type), header.retain, header.QoS, header.dup, (const char*)packetDump);
#endif
            return socket->sendReliably(buffer, (int)length, timeoutMs);
        }

        bool hasValidLength() const
        {
            Protocol::MQTT::Common::VBInt l;
            return l.readFrom(recvBuffer + 1, available - 1) != Protocol::MQTT::Common::BadData;
        }

        /** Receive a control packet from the socket in the given time.
            @retval positive    The number of bytes received
            @retval 0           Protocol error, you should close the socket
            @retval -1          Socket error
            @retval -2          Timeout */
        int receiveControlPacket(const bool lowLatency = false)
        {
            if (!socket) return -1;
            // Depending on the current state, we need to fetch as many bytes as possible within the given timeoutMs
            // This is a complex problem here because we want both to optimize for
            //  - latency (returns as fast as possible when we've received a complete packet)
            //  - blocking time (don't return immediately if the data is currently in transfer, need to wait for it to arrive)
            //  - minimal syscalls (don't call recv byte per byte as the overhead will be significant)
            //  - network queue (don't fetch more byte than necessary for getting a single control packet)
            //  - streaming usage (this can be called while a control packet was being received and we timed out)

            // So the algorithm used here depends on the current receiving state
            // If we haven't received packet length yet, we have to fetch the header very carefully
            // Else, we can enter a more general receiving loop until we have all bytes from the control packet
            int ret = 0;
            Protocol::MQTT::Common::VBInt len;

#if MQTTLowLatency == 1
            // In low latency mode, return as early as possible
            if (lowLatency && !socket->select(true, false, 0)) return -2;
#endif

            // We want to keep track of complete timeout time over multiple operations
            Time::TimeOut timeout(timeoutMs);
            switch (recvState)
            {
            case Ready:
            case GotType:
            {   // Here, make sure we only fetch the length first
                // The minimal size is 2 bytes for PINGRESP, DISCONNECT and AUTH.
                // Because of this, we can't really outsmart the system everytime
                ret = socket->receiveReliably((char*)&recvBuffer[available], 2 - available, timeout);
                if (ret > 0) available += ret;
                // Deal with timeout first
                if (timeout == 0) return -2;
                // Deal with socket errors here
                if (ret < 0 || available < 2) return -1;
                // Depending on the packet type, let's wait for more data
                if (recvBuffer[0] < 0xD0 || recvBuffer[1]) // Below ping response or packet size larger than 2 bytes
                {
                    int querySize = (packetExpectedVBSize + 1) - available;
                    ret = socket->receiveReliably((char*)&recvBuffer[available], querySize, timeout);
                    if (ret > 0) available += ret;
                    // Deal with timeout first
                    if (timeout == 0) return -2;
                    // Deal with socket errors here
                    if (ret < 0) return ret;
                }

                recvState = GotLength;
                break;
            }
            default: break;
            }
            // Here we should either have a valid control packet header
            uint32 r = len.readFrom(&recvBuffer[1], available - 1);
            if (r == Protocol::MQTT::Common::BadData)
                return 0; // Close the socket here, the given data are wrong or not the right protocol
            if (r == Protocol::MQTT::Common::NotEnoughData)
            {
                if (available >= (packetExpectedVBSize+1))
                {   // The server sends us a packet that's larger than the expected maximum size,
                    // In MQTTv5 it's a protocol error, so let's disconnect
                    return 0;
                }
                // We haven't received enough data in the given timeout to make progress, let's report a timeout
                recvState = GotType;
                return -2;
            }
            uint32 remainingLength = len;
            uint32 totalPacketSize = remainingLength + 1 + len.getSize();
            ret = totalPacketSize == available ? 0 : socket->receiveReliably((char*)&recvBuffer[available], (totalPacketSize - available), timeout);
            if (ret > 0) available += ret;
            if (timeout == 0) return -2;
            if (ret < 0) return ret;

            // Ok, let's check if we have received the complete packet
            if (available == totalPacketSize)
            {
                recvState = GotCompletePacket;
#if MQTTDumpCommunication == 1
                // Dump the packet received
                String packetDump;
                Utils::hexDump(packetDump, recvBuffer, available, 16, true, true);
                Protocol::MQTT::V5::FixedHeader header;
                header.raw = recvBuffer[0];
                Logger::log(Logger::Dump, "< Received packet: %s(R:%d,Q:%d,D:%d)%s", Protocol::MQTT::V5::Helper::getControlPacketName((Protocol::MQTT::Common::ControlPacketType)(uint8)header.type), header.retain, header.QoS, header.dup, (const char*)packetDump);
#endif
                lastCommunication = (uint32)time(NULL);
                return (int)available;
            }
            // No yet, but we probably timed-out.
            return -2;
        }

        /** Get the last received packet type */
        Protocol::MQTT::V5::ControlPacketType getLastPacketType() const
        {
            if (recvState != GotCompletePacket) return Protocol::MQTT::V5::RESERVED;
            Protocol::MQTT::V5::FixedHeader header;
            header.raw = recvBuffer[0];
            return (Protocol::MQTT::V5::ControlPacketType)(uint8)header.type;
        }

        /** Extract a control packet of the given type */
        int extractControlPacket(const Protocol::MQTT::V5::ControlPacketType type, Protocol::MQTT::Common::Serializable & packet)
        {
            if (recvState != GotCompletePacket)
            {
                int ret = receiveControlPacket();
                if (ret <= 0) return ret;

                if (recvState != GotCompletePacket)
                    return -2;
            }

            // Check the packet is the last expected type
            if (getLastPacketType() != type) return -3;

            // Seems to be the expected type, let's unserialize it
            uint32 r = packet.readFrom(recvBuffer, recvBufferSize);
            if (Protocol::MQTT::Common::isError(r)) return -4; // Parsing error

            // Done with receiving the packet let's remember it
            resetPacketReceivingState();

#if MQTTDumpCommunication == 1
            MQTTString out;
            packet.dump(out, 2);
            Logger::log(Logger::Dump, "Received\n%.*s", MQTTStringGetLength(out), MQTTStringGetData(out));
#endif

            return (int)r;
        }

        void resetPacketReceivingState() { recvState = Ready; available = 0; }

        void close()
        {
            delete0(socket);
        }

        bool isOpen()
        {
            return socket != nullptr;
        }

        int connectWith(const char * host, const uint16 port, const bool withTLS)
        {
            if (isOpen()) return -1;
            if (withTLS)
            {
  #if MQTTUseTLS == 1
                if (!sslContext)
                {   // If one certificate is given let's use it instead of the default CA bundle
                    sslContext = brokerCert ? new SSLContext(NULL, Crypto::SSLContext::Any) : new SSLContext();
                }
                if (!sslContext) return -2;
                // Insert here any session specific configuration or certificate validator
                if (brokerCert)
                {
                    if (const char * error = sslContext->loadCertificateFromDER(brokerCert->data, brokerCert->length))
                    {
                        Logger::log(Logger::Error, "Could not load the given certificate: %s", error);
                        return -2;
                    }
                }
                socket = new Network::Socket::SSL_TLS(*sslContext, Network::Socket::BaseSocket::Stream);
  #else
                return -1;
  #endif
            } else socket = new Socket(Network::Socket::BaseSocket::Stream);

            if (!socket) return -2;
            // Let the socket be asynchronous and without Nagle's algorithm
            if (!socket->setOption(Network::Socket::BaseSocket::Blocking, 0)) return -3;
            if (!socket->setOption(Network::Socket::BaseSocket::NoDelay, 1)) return -4;
            if (!socket->setOption(Network::Socket::BaseSocket::NoSigPipe, 1)) return -5;
            // Then connect the socket to the server
            int ret = socket->connect(Network::Address::URL("", String(host) + ":" + port, ""));
            if (ret < 0) return -6;
            if (ret == 0) return 0;

            // Here, we need to wait until connection happens or times out
            if (socket->select(false, true, timeoutMs)) return 0;
            return -7;
        }

#if MQTTUseAuth == 1
        MQTTv5::ErrorType handleAuth()
        {
            Protocol::MQTT::V5::ROAuthPacket packet;
            int ret = extractControlPacket(Protocol::MQTT::V5::AUTH, packet);
            if (ret > 0)
            {
                // Parse the Auth packet and call the user method
                // Try to find the auth method, and the auth data
                DynamicStringView authMethod;
                DynamicBinDataView authData;
                Protocol::MQTT::V5::VisitorVariant visitor;
                while (packet.props.getProperty(visitor) && (authMethod.length == 0 || authData.length == 0))
                {
                    if (visitor.propertyType() == Protocol::MQTT::V5::AuthenticationMethod)
                    {
                        auto view = visitor.as< DynamicStringView >();
                        authMethod = *view;
                    }
                    else if (visitor.propertyType() == Protocol::MQTT::V5::AuthenticationData)
                    {
                        auto data = visitor.as< DynamicBinDataView >();
                        authData = *data;
                    }
                }
                return cb->authReceived(packet.fixedVariableHeader.reason(), authMethod, authData, packet.props) ? MQTTv5::ErrorType::Success : MQTTv5::ErrorType::NetworkError;
            }
            return ErrorType::NetworkError;
        }
#endif
        MQTTv5::ErrorType handleConnACK()
        {
            // Parse the ConnACK packet;
            Protocol::MQTT::V5::ROConnACKPacket packet;
            int ret = extractControlPacket(Protocol::MQTT::V5::CONNACK, packet);
            if (ret > 0)
            {
                // We are only interested in the result of the connection
                if (packet.fixedVariableHeader.acknowledgeFlag & 1)
                {   // Session is present on the server. For now, we don't care, do we ?

                }
                if (packet.fixedVariableHeader.reasonCode != 0
#if MQTTUseAuth == 1
                    && packet.fixedVariableHeader.reasonCode != Protocol::MQTT::V5::NotAuthorized
                    && packet.fixedVariableHeader.reasonCode != Protocol::MQTT::V5::BadAuthenticationMethod
#endif
                )
                {
                    // We have failed connection with the following reason:
                    return (MQTTv5::ReasonCodes)packet.fixedVariableHeader.reasonCode;
                }
                // Now, we are going to parse the other properties
#if MQTTUseAuth == 1
                DynamicStringView authMethod;
                DynamicBinDataView authData;
#endif
                Protocol::MQTT::V5::VisitorVariant visitor;
                while (packet.props.getProperty(visitor))
                {
                    switch (visitor.propertyType())
                    {
                    case Protocol::MQTT::V5::PacketSizeMax:
                    {
                        auto pod = visitor.as< Protocol::MQTT::V5::LittleEndianPODVisitor<uint32> >();
                        maxPacketSize = pod->getValue();
                        break;
                    }
                    case Protocol::MQTT::V5::AssignedClientID:
                    {
                        auto view = visitor.as< Protocol::MQTT::V5::DynamicStringView >();
                        clientID.from(view->data, view->length); // This allocates memory for holding the copy
                        break;
                    }
                    case Protocol::MQTT::V5::ServerKeepAlive:
                    {
                        auto pod = visitor.as< Protocol::MQTT::V5::LittleEndianPODVisitor<uint16> >();
                        keepAlive = (pod->getValue() + (pod->getValue()>>1)) >> 1; // Use 0.75 of the server's told value
                        break;
                    }
#if MQTTUseAuth == 1
                    case Protocol::MQTT::V5::AuthenticationMethod:
                    {
                        auto view = visitor.as<DynamicStringView>();
                        authMethod = *view;
                    } break;
                    case Protocol::MQTT::V5::AuthenticationData:
                    {
                        auto data = visitor.as<DynamicBinDataView>();
                        authData = *data;
                    } break;
#endif
                    // Actually, we don't care about other properties. Maybe we should ?
                    default: break;
                    }
                }
#if MQTTUseAuth == 1
                if (packet.fixedVariableHeader.reasonCode == Protocol::MQTT::V5::NotAuthorized
                 || packet.fixedVariableHeader.reasonCode == Protocol::MQTT::V5::BadAuthenticationMethod)
                {   // Let the user be aware of the required authentication properties so next connect will/can contains them
                    return cb->authReceived((ReasonCodes)packet.fixedVariableHeader.reasonCode, authMethod, authData, packet.props) ? ErrorType::Success : ErrorType::NetworkError;
                }
#endif
                return ErrorType::Success;
            }
            return Protocol::MQTT::V5::ProtocolError;
        }

    };
#else
#ifndef MQTTLock
    /* If you have a true lock object in your system (for example, in FreeRTOS, use a mutex),
       you should provide one instead of this one as this one just burns CPU while waiting */
    class SpinLock
    {
        mutable std::atomic<bool> state;
    public:
        /** Construction */
        SpinLock() : state(false) {}
        /** Acquire the lock */
        inline void acquire() volatile
        {
            while (state.exchange(true, std::memory_order_acq_rel))
            {
                // Put a sleep method here (using select here since it's cross platform in BSD socket API)
                struct timeval tv;
                tv.tv_sec = 0; tv.tv_usec = 500; // Wait 0.5ms per loop
                select(0, NULL, NULL, NULL, &tv);
            }
        }
        /** Try to acquire the lock */
        inline bool tryAcquire() volatile { return state.exchange(true, std::memory_order_acq_rel) == false; }
        /** Check if the lock is taken. For debugging purpose only */
        inline bool isLocked() const volatile { return state.load(std::memory_order_consume); }
        /** Release the lock */
        inline void release() volatile { state.store(false, std::memory_order_release);; }
    };

    typedef SpinLock Lock;
    struct ScopedLock
    {
        Lock & a;
        ScopedLock(Lock & a) : a(a) { a.acquire(); }
        ~ScopedLock() { a.release(); }
    };
#else
    #define Lock        MQTTLock
    #define ScopedLock  MQTTScopedLock
#endif
#ifndef closesocket
    #define closesocket close
#endif

#ifndef MSG_NOSIGNAL
    #define MSG_NOSIGNAL 0
#endif

#if MQTTDumpCommunication == 1
    static void hexdump(const void * buf, int len)
    {
        for (int i = 0; i < len; i++)
            printf("%02X ", ((unsigned char*)buf)[i]);
        printf("\n"); // Flush output
    }
    static void dumpBufferAsPacket(const char * prompt, const uint8* buffer, uint32 length)
    {
        // Dump the packet to send
        Protocol::MQTT::V5::FixedHeader header;
        header.raw = buffer[0];
        printf("%s: %s(R:%d,Q:%d,D:%d)\n", prompt, Protocol::MQTT::V5::Helper::getControlPacketName((Protocol::MQTT::Common::ControlPacketType)(uint8)header.type), (uint8)header.retain, (uint8)header.QoS, (uint8)header.dup);
        hexdump(buffer, length);
    }
#endif

#if MQTTUseTLS == 1
    // Small optimization to remove useless virtual table in the final binary if not used
    #define MQTTVirtual virtual
#else
    #define MQTTVirtual
#endif

    struct BaseSocket
    {
        int     socket;
        struct timeval &         timeoutMs;

        MQTTVirtual int connect(const char * host, uint16 port, const MQTTv5::DynamicBinDataView *)
        {
            socket = ::socket(AF_INET, SOCK_STREAM, 0);
            if (socket == -1) return -2;

            // Please notice that under linux, it's not required to set the socket
            // as non blocking if you define SO_SNDTIMEO, for connect timeout.
            // so the code below could be optimized away. Yet, lwIP does show the
            // same behavior so when a timeout for connection is actually required
            // you must issue a select call here.

            // Set non blocking here
            int socketFlags = ::fcntl(socket, F_GETFL, 0);
            if (socketFlags == -1) return -3;
            if (::fcntl(socket, F_SETFL, (socketFlags | O_NONBLOCK)) != 0) return -3;

            // Let the socket be without Nagle's algorithm
            int flag = 1;
            if (::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) return -4;
            // Then connect the socket to the server
            struct addrinfo hints = {};
            hints.ai_family = AF_INET; // IPv4 only for now
            hints.ai_flags = AI_ADDRCONFIG;
            hints.ai_socktype = SOCK_STREAM;

            // Resolve address
            struct addrinfo *result = NULL;
            if (getaddrinfo(host, NULL, &hints, &result) < 0 || result == NULL) return -5;

            // Then connect to it
            struct sockaddr_in address;
            address.sin_port = htons(port);
            address.sin_family = AF_INET;
            address.sin_addr = ((struct sockaddr_in *)(result->ai_addr))->sin_addr;

            // free result
            freeaddrinfo(result);
            int ret = ::connect(socket, (const sockaddr*)&address, sizeof(address));
            if (ret < 0 && errno != EINPROGRESS) return -6;
            if (ret == 0) return 0;

            // Here, we need to wait until connection happens or times out
            if (select(false, true))
            {
                // Restore blocking behavior here
                if (::fcntl(socket, F_SETFL, socketFlags) != 0) return -3;
                // And set timeouts for both recv and send
                if (::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeoutMs, sizeof(timeoutMs)) < 0) return -4;
                if (::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeoutMs, sizeof(timeoutMs)) < 0) return -4;
                // Ok, done!
                return 0;
            }

            return -7;
        }
        MQTTVirtual int recv(char * buffer, const uint32 minLength, const uint32 maxLength = 0)
        {
            int ret = ::recv(socket, buffer, minLength, MSG_WAITALL);
            if (ret <= 0) return ret;
            if (maxLength <= minLength) return ret;

            int nret = ::recv(socket, &buffer[ret], maxLength - ret, 0);
            return nret <= 0 ? nret : nret + ret;
        }

        MQTTVirtual int send(const char * buffer, const uint32 length)
        {
#if MQTTDumpCommunication == 1
            dumpBufferAsPacket("> Sending packet", (const uint8*)buffer, length);
#endif
            return ::send(socket, buffer, (int)length, 0);
        }

        // Useful socket helpers functions here
        MQTTVirtual int select(bool reading, bool writing, bool instantaneous = false)
        {
            // Linux modifies the timeout when calling select
            struct timeval v = timeoutMs;
            if (instantaneous) memset(&v, 0, sizeof(v));

            fd_set set;
            FD_ZERO(&set);
            FD_SET(socket, &set);
            // Then select
            return ::select(socket + 1, reading ? &set : NULL, writing ? &set : NULL, NULL, &v);
        }

        BaseSocket(struct timeval & timeoutMs) : socket(-1), timeoutMs(timeoutMs) {}
        MQTTVirtual ~BaseSocket() { ::closesocket(socket); socket = -1; }
    };


#if MQTTUseTLS == 1
    class MBTLSSocket : public BaseSocket
    {
        mbedtls_entropy_context entropy;
        mbedtls_ctr_drbg_context entropySource;
        mbedtls_ssl_context ssl;
        mbedtls_ssl_config conf;
        mbedtls_x509_crt cacert;
        mbedtls_net_context net;

    private:
        bool buildConf(const MQTTv5::DynamicBinDataView * brokerCert)
        {
            if (brokerCert)
            {   // Use given root certificate (if you have a recent version of mbedtls, you could use mbedtls_x509_crt_parse_der_nocopy instead to skip a useless copy here)
                if (::mbedtls_x509_crt_parse_der(&cacert, brokerCert->data, brokerCert->length))
                    return false;
            }

            // Now create configuration from default
            if (::mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT))
                return false;

            ::mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
            ::mbedtls_ssl_conf_authmode(&conf, brokerCert ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE);

            uint32_t ms = timeoutMs.tv_usec / 1000;
            ::mbedtls_ssl_conf_read_timeout(&conf, ms < 50 ? 3000 : ms);

            // Random number generator
            ::mbedtls_ssl_conf_rng(&conf, ::mbedtls_ctr_drbg_random, &entropySource);
            if (::mbedtls_ctr_drbg_seed(&entropySource, ::mbedtls_entropy_func, &entropy, NULL, 0))
                return false;

            if (::mbedtls_ssl_setup(&ssl, &conf))
                return false;

            return true;
        }

    public:
        MBTLSSocket(struct timeval & timeoutMs) : BaseSocket(timeoutMs)
        {
            mbedtls_ssl_init(&ssl);
            mbedtls_ssl_config_init(&conf);
            mbedtls_x509_crt_init(&cacert);
            mbedtls_ctr_drbg_init(&entropySource);
            mbedtls_entropy_init(&entropy);
        }

        int connect(const char * host, uint16 port, const MQTTv5::DynamicBinDataView * brokerCert)
        {
            int ret = BaseSocket::connect(host, port, 0);
            if (ret) return ret;

            // MBedTLS doesn't deal with natural socket timeout correctly, so let's fix that
            struct timeval zeroTO = {};
            if (::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &zeroTO, sizeof(zeroTO)) < 0) return -4;
            if (::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &zeroTO, sizeof(zeroTO)) < 0) return -4;

            net.fd = socket;

            if (!buildConf(brokerCert))                                             return -8;
            if (::mbedtls_ssl_set_hostname(&ssl, host))                             return -9;

            // Set the method the SSL engine is using to fetch/send data to the other side
            ::mbedtls_ssl_set_bio(&ssl, &net, ::mbedtls_net_send, NULL, ::mbedtls_net_recv_timeout);

            ret = ::mbedtls_ssl_handshake(&ssl);
            if (ret != 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
                return -10;

            // Check certificate if one provided
            if (brokerCert)
            {
                uint32_t flags = mbedtls_ssl_get_verify_result(&ssl);
                if (flags != 0)
                {
#if MQTTDumpCommunication == 1
                    char verify_buf[100] = {0};
                    mbedtls_x509_crt_verify_info(verify_buf, sizeof(verify_buf), "  ! ", flags);
                    printf("mbedtls_ssl_get_verify_result: %s flag: 0x%x\n", verify_buf, (unsigned int)flags);
#endif
                    return -11;
                }
            }
            return 0;
        }

        int send(const char * buffer, const uint32 length)
        {
#if MQTTDumpCommunication == 1
            dumpBufferAsPacket("> Sending packet", (const uint8*)buffer, length);
#endif
            return ::mbedtls_ssl_write(&ssl, (const uint8*)buffer, length);
        }

        int recv(char * buffer, const uint32 minLength, const uint32 maxLength = 0)
        {
            uint32 ret = 0;
            while (ret < minLength)
            {
                int r = ::mbedtls_ssl_read(&ssl, (uint8*)&buffer[ret], minLength - ret);
                if (r <= 0)
                {
                    // Those means that we need to call again the read method
                    if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE)
                        continue;
                    if (r == MBEDTLS_ERR_SSL_TIMEOUT) {
                        errno = EWOULDBLOCK; // Remember it's a timeout
                        return -1;
                    }
                    return ret ? (int)ret : r; // Silent error here
                }
                ret += (uint32)r;
            }
            if (maxLength <= minLength) return ret;

            // This one is a non blocking call
            int nret = ::mbedtls_ssl_read(&ssl, (uint8*)&buffer[ret], maxLength - ret);
            if (nret == MBEDTLS_ERR_SSL_TIMEOUT) return ret;
            return nret <= 0 ? nret : nret + ret;
        }

        ~MBTLSSocket()
        {
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_x509_crt_free(&cacert);
            mbedtls_entropy_free(&entropy);
            mbedtls_ssl_config_free(&conf);
            mbedtls_ctr_drbg_free(&entropySource);
            mbedtls_ssl_free(&ssl);
        }
    };
#endif

    struct MQTTv5::Impl
    {
        /** The multithread protection for this object */
        Lock                        lock;
        /** This client socket */
        BaseSocket *                socket;
        /** The DER encoded certificate (if provided) */
        const DynamicBinDataView *  brokerCert;
        /** This client unique identifier */
        DynamicString               clientID;
        /** The message received callback to use */
        MessageReceived *           cb;
        /** The default timeout in milliseconds */
        struct timeval              timeoutMs;

        /** The last communication time in second */
        uint32                      lastCommunication;
        /** The publish current default identifier allocator */
        uint16                      publishCurrentId;
        /** The keep alive delay in seconds */
        uint16                      keepAlive;
#if MQTTUseAuth == 1
        /** Mask used to track the origin of the AUTH exchange and reentrancy issues */
        uint32                      authSource;
#endif
#if MQTTUseUnsubscribe == 1
        /** The last unsubscribe id */
        uint16                      unsubscribeId;
        /** The last unsubscribe error code */
        MQTTv5::ErrorType::Type     lastUnsubscribeError;
#endif

        /** The reading state. Because data on a TCP stream is
            a stream, we have to remember what state we are currently following while parsing data */
        enum RecvState
        {
            Ready = 0,
            GotType,
            GotLength,
            GotCompletePacket,
        }                   recvState;
        /** The receiving buffer size */
        uint32              recvBufferSize;
        /** The maximum packet size the server is willing to accept */
        uint32              maxPacketSize;
        /** The available data in the buffer */
        uint32              available;
        /** The receiving data buffer */
        uint8   *           recvBuffer;
        /** The receiving VBInt size for the packet header */
        uint8               packetExpectedVBSize;

        uint16 allocatePacketID()
        {
            return ++publishCurrentId;
        }

        Impl(const char * clientID, MessageReceived * callback, const DynamicBinDataView * brokerCert)
             : socket(0), brokerCert(brokerCert), clientID(clientID), cb(callback), timeoutMs({3, 0}), lastCommunication(0), publishCurrentId(0), keepAlive(300),
#if MQTTUseAuth == 1
               authSource(0),
#endif
#if MQTTUseUnsubscribe == 1
               unsubscribeId(0), lastUnsubscribeError(ErrorType::WaitingForResult),
#endif
               recvState(Ready), recvBufferSize(max(callback->maxPacketSize(), 8U)), maxPacketSize(65535), available(0), recvBuffer((uint8*)::malloc(recvBufferSize)), packetExpectedVBSize(Protocol::MQTT::Common::VBInt(recvBufferSize).getSize())
        {}
        ~Impl() { delete0(socket); ::free(recvBuffer); recvBuffer = 0; recvBufferSize = 0; }

        inline void setTimeout(uint32 timeout)
        {
            timeoutMs.tv_sec = (uint32)timeout / 1024; // Avoid division here (compiler should shift the value here), the value is approximative anyway
            timeoutMs.tv_usec = ((uint32)timeout & 1023) * 977;  // Avoid modulo here and make sure it doesn't overflow (since 1023 * 977 < 1000000)
        }

        bool shouldPing()
        {
            return (((uint32)time(NULL) - lastCommunication) >= keepAlive);
        }

        bool hasValidLength() const
        {
            Protocol::MQTT::Common::VBInt l;
            return l.readFrom(recvBuffer + 1, available - 1) != Protocol::MQTT::Common::BadData;
        }

        /** Receive a control packet from the socket in the given time.
            @retval positive    The number of bytes received
            @retval 0           Protocol error, you should close the socket
            @retval -1          Socket error
            @retval -2          Timeout */
        int receiveControlPacket(const bool lowLatency = false)
        {
            if (!socket) return -1;
            // Depending on the current state, we need to fetch as many bytes as possible within the given timeoutMs
            // This is a complex problem here because we want both to optimize for
            //  - latency (returns as fast as possible when we've received a complete packet)
            //  - blocking time (don't return immediately if the data is currently in transfer, need to wait for it to arrive)
            //  - minimal syscalls (don't call recv byte per byte as the overhead will be significant)
            //  - network queue (don't fetch more byte than necessary for getting a single control packet)
            //  - streaming usage (this can be called while a control packet was being received and we timed out)

            // So the algorithm used here depends on the current receiving state
            // If we haven't received packet length yet, we have to fetch the header very carefully
            // Else, we can enter a more general receiving loop until we have all bytes from the control packet
            int ret = 0;
            Protocol::MQTT::Common::VBInt len;

#if MQTTLowLatency == 1
            // In low latency mode, return as early as possible
            if (lowLatency && !socket->select(true, false, true)) return -2;
#endif

            // We want to keep track of complete timeout time over multiple operations
            switch (recvState)
            {
            case Ready:
            case GotType:
            {   // Here, make sure we only fetch the length first
                // The minimal size is 2 bytes for PINGRESP and shortcut DISCONNECT / AUTH.
                // Because of this, we can't really outsmart the system everytime
                ret = socket->recv((char*)&recvBuffer[available], 2 - available);
                if (ret > 0) available += ret;
                // Deal with timeout first
                if (ret < 0 || available < 2)
                    return (errno == EWOULDBLOCK) ? -2 : -1;

                // Depending on the packet type, let's wait for more data
                if (recvBuffer[0] < 0xD0 || recvBuffer[1]) // Below ping response or packet size larger than 2 bytes
                {
                    int querySize = (packetExpectedVBSize + 1) - available;
                    ret = socket->recv((char*)&recvBuffer[available], querySize);
                    if (ret > 0) available += ret;
                    // Deal with timeout first
                    if (ret < 0) return (errno == EWOULDBLOCK) ? -2 : -1;
                }

                recvState = GotLength;
                break;
            }
            default: break;
            }
            // Here we should either have a valid control packet header
            uint32 r = len.readFrom(&recvBuffer[1], available - 1);
            if (r == Protocol::MQTT::Common::BadData)
                return 0; // Close the socket here, the given data are wrong or not the right protocol
            if (r == Protocol::MQTT::Common::NotEnoughData)
            {
                if (available >= (packetExpectedVBSize+1))
                {   // The server sends us a packet that's larger than the expected maximum size,
                    // In MQTTv5 it's a protocol error, so let's disconnect
                    return 0;
                }
                // We haven't received enough data in the given timeout to make progress, let's report a timeout
                recvState = GotType;
                return -2;
            }
            uint32 remainingLength = len;
            uint32 totalPacketSize = remainingLength + 1 + len.getSize();
            ret = totalPacketSize == available ? 0 : socket->recv((char*)&recvBuffer[available], totalPacketSize - available);
            if (ret > 0) available += ret;
            if (ret < 0) return (errno == EWOULDBLOCK) ? -2 : -1;

            // Ok, let's check if we have received the complete packet
            if (available == totalPacketSize)
            {
                recvState = GotCompletePacket;
#if MQTTDumpCommunication == 1
                dumpBufferAsPacket("< Received packet", recvBuffer, available);
#endif
                lastCommunication = (uint32)time(NULL);
                return (int)available;
            }
            // No yet, but we probably timed-out.
            return -2;
        }

        /** Get the last received packet type */
        Protocol::MQTT::V5::ControlPacketType getLastPacketType() const
        {
            if (recvState != GotCompletePacket) return Protocol::MQTT::V5::RESERVED;
            Protocol::MQTT::V5::FixedHeader header;
            header.raw = recvBuffer[0];
            return (Protocol::MQTT::V5::ControlPacketType)(uint8)header.type;
        }

        /** Extract a control packet of the given type */
        int extractControlPacket(const Protocol::MQTT::V5::ControlPacketType type, Protocol::MQTT::Common::Serializable & packet)
        {
            if (recvState != GotCompletePacket)
            {
                int ret = receiveControlPacket();
                if (ret <= 0) return ret;

                if (recvState != GotCompletePacket)
                    return -2;
            }

            // Check the packet is the last expected type
            if (getLastPacketType() != type) return -3;

            // Seems to be the expected type, let's unserialize it
            uint32 r = packet.readFrom(recvBuffer, recvBufferSize);
            if (Protocol::MQTT::Common::isError(r)) return -4; // Parsing error

            // Done with receiving the packet let's remember it
            resetPacketReceivingState();

#if MQTTDumpCommunication == 1
//            String out;
//            packet.dump(out, 2);
//            printf("Received\n%s\n", (const char*)out);
#endif

            return (int)r;
        }

        void resetPacketReceivingState() { recvState = Ready; available = 0; }

        void close()
        {
            delete0(socket);
        }

        bool isOpen()
        {
            return socket;
        }

        int send(const char * buffer, const int size) { return socket ? socket->send(buffer, size) : -1; }

        int connectWith(const char * host, const uint16 port, const bool withTLS)
        {
            if (isOpen()) return -1;
            socket =
#if MQTTUseTLS == 1
                withTLS ? new MBTLSSocket(timeoutMs) :
#endif
                new BaseSocket(timeoutMs);
            return socket ? socket->connect(host, port, brokerCert) : -1;
        }

#if MQTTUseAuth == 1
        MQTTv5::ErrorType handleAuth()
        {
            Protocol::MQTT::V5::ROAuthPacket packet;
            int ret = extractControlPacket(Protocol::MQTT::V5::AUTH, packet);
            if (ret > 0)
            {
                // Parse the Auth packet and call the user method
                // Try to find the auth method, and the auth data
                DynamicStringView authMethod;
                DynamicBinDataView authData;
                Protocol::MQTT::V5::VisitorVariant visitor;
                while (packet.props.getProperty(visitor) && (authMethod.length == 0 || authData.length == 0))
                {
                    if (visitor.propertyType() == Protocol::MQTT::V5::AuthenticationMethod)
                    {
                        auto view = visitor.as< DynamicStringView >();
                        authMethod = *view;
                    }
                    else if (visitor.propertyType() == Protocol::MQTT::V5::AuthenticationData)
                    {
                        auto data = visitor.as< DynamicBinDataView >();
                        authData = *data;
                    }
                }
                return cb->authReceived(packet.fixedVariableHeader.reason(), authMethod, authData, packet.props) ? MQTTv5::ErrorType::Success : MQTTv5::ErrorType::NetworkError;
            }
            return ErrorType::NetworkError;
        }
#endif
        MQTTv5::ErrorType handleConnACK()
        {
            // Parse the ConnACK packet;
            Protocol::MQTT::V5::ROConnACKPacket packet;
            int ret = extractControlPacket(Protocol::MQTT::V5::CONNACK, packet);
            if (ret > 0)
            {
                // We are only interested in the result of the connection
                if (packet.fixedVariableHeader.acknowledgeFlag & 1)
                {   // Session is present on the server. For now, we don't care, do we ?

                }
                if (packet.fixedVariableHeader.reasonCode != 0
#if MQTTUseAuth == 1
                    && packet.fixedVariableHeader.reasonCode != Protocol::MQTT::V5::NotAuthorized
                    && packet.fixedVariableHeader.reasonCode != Protocol::MQTT::V5::BadAuthenticationMethod
#endif
                )
                {
                    // We have failed connection with the following reason:
                    return (MQTTv5::ReasonCodes)packet.fixedVariableHeader.reasonCode;
                }
                // Now, we are going to parse the other properties
#if MQTTUseAuth == 1
                DynamicStringView authMethod;
                DynamicBinDataView authData;
#endif
                Protocol::MQTT::V5::VisitorVariant visitor;
                while (packet.props.getProperty(visitor))
                {
                    switch (visitor.propertyType())
                    {
                    case Protocol::MQTT::V5::PacketSizeMax:
                    {
                        auto pod = visitor.as< Protocol::MQTT::V5::LittleEndianPODVisitor<uint32> >();
                        maxPacketSize = pod->getValue();
                        break;
                    }
                    case Protocol::MQTT::V5::AssignedClientID:
                    {
                        auto view = visitor.as< Protocol::MQTT::V5::DynamicStringView >();
                        clientID.from(view->data, view->length); // This allocates memory for holding the copy
                        break;
                    }
                    case Protocol::MQTT::V5::ServerKeepAlive:
                    {
                        auto pod = visitor.as< Protocol::MQTT::V5::LittleEndianPODVisitor<uint16> >();
                        keepAlive = (pod->getValue() + (pod->getValue()>>1)) >> 1; // Use 0.75 of the server's told value
                        break;
                    }
#if MQTTUseAuth == 1
                    case Protocol::MQTT::V5::AuthenticationMethod:
                    {
                        auto view = visitor.as<DynamicStringView>();
                        authMethod = *view;
                    } break;
                    case Protocol::MQTT::V5::AuthenticationData:
                    {
                        auto data = visitor.as<DynamicBinDataView>();
                        authData = *data;
                    } break;
#endif
                    // Actually, we don't care about other properties. Maybe we should ?
                    default: break;
                    }
                }
#if MQTTUseAuth == 1
                if (packet.fixedVariableHeader.reasonCode == Protocol::MQTT::V5::NotAuthorized
                 || packet.fixedVariableHeader.reasonCode == Protocol::MQTT::V5::BadAuthenticationMethod)
                {   // Let the user be aware of the required authentication properties so next connect will/can contains them
                    return cb->authReceived((ReasonCodes)packet.fixedVariableHeader.reasonCode, authMethod, authData, packet.props) ? ErrorType::Success : ErrorType::NetworkError;
                }
#endif
                return ErrorType::Success;
            }
            return Protocol::MQTT::V5::ProtocolError;
        }
    };
#endif

    MQTTv5::MQTTv5(const char * clientID, MessageReceived * callback, const DynamicBinDataView * brokerCert) : impl(new Impl(clientID, callback, brokerCert)) {}
    MQTTv5::~MQTTv5() { delete impl; impl = 0; }

    MQTTv5::ErrorType::Type MQTTv5::prepareSAR(Protocol::MQTT::V5::ControlPacketSerializable & packet, bool withAnswer)
    {
        // Ok, setting are done, let's build this packet now
        uint32 packetSize = packet.computePacketSize();
        DeclareStackHeapBuffer(buffer, packetSize, StackSizeAllocationLimit);
        if (packet.copyInto(buffer) != packetSize)
            return ErrorType::UnknownError;

#if MQTTDumpCommunication == 1
//      String out;
//      packet.dump(out, 2);
//      printf("Prepared:\n%s\n", (const char*)out);
#endif
        // Make sure we are on a clean receiving state
        impl->resetPacketReceivingState();

        if (impl->send((const char*)buffer, packetSize) != packetSize)
            return ErrorType::NetworkError;

        if (!withAnswer) return ErrorType::Success;

        // Next, we'll wait for server's CONNACK or AUTH coming here (or error)
        int receivedPacketSize = impl->receiveControlPacket();
        if (receivedPacketSize <= 0)
        {   // This will also comes here
            if (receivedPacketSize == 0) impl->close();
            return receivedPacketSize == -2 ? ErrorType::TimedOut : ErrorType::NetworkError;
        }
        return ErrorType::Success;
    }

    // Connect to the given server URL.
    MQTTv5::ErrorType MQTTv5::connectTo(const char * serverHost, const uint16 port, bool useTLS, const uint16 keepAliveTimeInSec,
        const bool cleanStart, const char * userName, const DynamicBinDataView * password, WillMessage * willMessage, const QoSDelivery willQoS, const bool willRetain,
        Properties * properties)
    {
        if (serverHost == nullptr || !port)
            return ErrorType::BadParameter;

        // Please do not move the line below as it must outlive the packet
        Protocol::MQTT::V5::Property<uint32> maxProp(Protocol::MQTT::V5::PacketSizeMax, impl->recvBufferSize);
        Protocol::MQTT::V5::ControlPacket<Protocol::MQTT::V5::CONNECT> packet;

        ScopedLock scope(impl->lock);
        if (impl->isOpen()) return ErrorType::AlreadyConnected;

        // Capture properties (to avoid copying them)
        packet.props.capture(properties);

        // Check if we have a max packet size property and if not, append one to let the server know our limitation (if any)
        if (impl->recvBufferSize < Protocol::MQTT::Common::VBInt::MaxPossibleSize)
            packet.props.append(&maxProp); // It'll fail silently if it already exists

#if MQTTAvoidValidation != 1
        if (!packet.props.checkPropertiesFor(Protocol::MQTT::V5::CONNECT))
            return ErrorType::BadProperties;
#endif

        // Check we can contact the server and connect to it (not initial write to the server is required here)
        if (int ret = impl->connectWith(serverHost, port, useTLS))
        {
            impl->close();
            return ret == -7 ? ErrorType::TimedOut : ErrorType::NetworkError;
        }

        // Create the header object now
        impl->keepAlive = (keepAliveTimeInSec + (keepAliveTimeInSec / 2)) / 2; // Make it 75% of what's given so we always wake up before doom's clock
        packet.fixedVariableHeader.keepAlive = keepAliveTimeInSec;
        packet.fixedVariableHeader.cleanStart = cleanStart ? 1 : 0;
        packet.fixedVariableHeader.willFlag = willMessage != nullptr ? 1 : 0;
        packet.fixedVariableHeader.willQoS = (uint8)willQoS;
        packet.fixedVariableHeader.willRetain = willRetain ? 1 : 0;
        packet.fixedVariableHeader.passwordFlag = password != nullptr ? 1 : 0;
        packet.fixedVariableHeader.usernameFlag = userName != nullptr ? 1 : 0;

        // And the payload too now
        packet.payload.clientID = impl->clientID;
        if (willMessage != nullptr) packet.payload.willMessage = willMessage;
        if (userName != nullptr)    packet.payload.username = userName;
        if (password != nullptr)    packet.payload.password = *password;

        // Ok, setting are done, let's build this packet now
        if (ErrorType ret = prepareSAR(packet))
        {
            impl->close();
            return ret;
        }

        // Then extract the packet type
        Protocol::MQTT::V5::ControlPacketType type = impl->getLastPacketType();
        if (type == Protocol::MQTT::V5::CONNACK)
        {
            ErrorType ret = impl->handleConnACK();
#if MQTTUseAuth == 1
            // No special treatment anymore for AUTH packets
            impl->authSource = 0;
#endif
            if (ret != ErrorType::Success)
                impl->close();
            return ret;
        }
#if MQTTUseAuth == 1
        else if (type == Protocol::MQTT::V5::AUTH)
        {
            // Authentication need to know if we are in a CONNECT/CONNACK process since it behaves differently in that case
            impl->authSource |= FromConnect | 1;

            if (impl->handleAuth() == ErrorType::Success)
            { // We need to receive either a CONNACK or a AUTH packet now, so let's do that until we're done
                while (true)
                {
                    // Ok, now we have a packet read it
                    type = impl->getLastPacketType();
                    if (type == Protocol::MQTT::V5::CONNACK)
                    {
                        ErrorType ret = impl->handleConnACK();
                        if (ret != ErrorType::Success)
                            impl->close();

                        impl->authSource = 0;
                        return ret;
                    }
                    else if (type == Protocol::MQTT::V5::AUTH)
                    {
                        if (ErrorType ret = impl->handleAuth())
                        {   // In case of authentication error, let's report back up
                            impl->close();
                            impl->authSource = 0;
                            return ret;
                        }   // Else, let's continue the Authentication dance
                    }
                    else
                    {
                        impl->close();
                        impl->authSource = 0;
                        return Protocol::MQTT::V5::ProtocolError;
                    }
                }
            }
        }
#endif
        // All other cases are error, let's close the connection now.
        impl->close();
        return Protocol::MQTT::V5::ProtocolError;
    }

#if MQTTUseAuth == 1
    struct ConditionalScopedLock
    {
        Lock * a;
        ConditionalScopedLock(Lock * a) : a(a) { if (a) a->acquire(); }
        ~ConditionalScopedLock() { if (a) a->release(); }
    };

    // Authenticate with the given server.
    MQTTv5::ErrorType MQTTv5::auth(const ReasonCodes reasonCode, const DynamicStringView & authMethod, const DynamicBinDataView & authData, Properties * properties)
    {
        if (reasonCode != Protocol::MQTT::V5::Success && reasonCode != Protocol::MQTT::V5::ContinueAuthentication && reasonCode != Protocol::MQTT::V5::ReAuthenticate)
            return ErrorType::BadParameter;
        if (!properties && (authMethod.length == 0 || authData.length == 0))
            return ErrorType::BadParameter; // A auth method is required

        // Don't move this around, it must appear on stack until it's no more used
        Protocol::MQTT::V5::Property<DynamicStringView> method(Protocol::MQTT::V5::AuthenticationMethod, authMethod);
        Protocol::MQTT::V5::Property<DynamicBinDataView> data(Protocol::MQTT::V5::AuthenticationData, authData);

        Protocol::MQTT::V5::AuthPacket packet;

        ConditionalScopedLock scope((impl->authSource & AuthMask) ? 0 : &impl->lock);
        if (!impl->isOpen()) return ErrorType::NotConnected;
        if (impl->getLastPacketType() != Protocol::MQTT::V5::RESERVED)
            return ErrorType::TranscientPacket;

        // Capture properties (to avoid copying them)
        packet.props.capture(properties);

        // Check if we have a auth method
        packet.props.append(&method); // That'll fail silently if it already exists
        packet.props.append(&data); // That'll fail silently if it already exists

        packet.fixedVariableHeader.reasonCode = reasonCode;

        // Then send the packet
        if (ErrorType ret = prepareSAR(packet))
            return ret;

        Protocol::MQTT::V5::ControlPacketType type = impl->getLastPacketType();
        if (type == Protocol::MQTT::V5::AUTH)
        {
            impl->authSource++;
            ErrorType ret = impl->handleAuth();
            impl->authSource--;
            return ret;
        } else if (type == Protocol::MQTT::V5::CONNACK && (impl->authSource & FromConnect) > 0)
        {   // We don't signal any error here, it's up to the parent's connectTo to check this packet
            return ErrorType::Success;
        }
        return Protocol::MQTT::V5::ProtocolError;
    }
#endif

    // Subscribe to a topic.
    MQTTv5::ErrorType MQTTv5::subscribe(const char * _topic, const RetainHandling retainHandling, const bool withAutoFeedBack, const QoSDelivery maxAcceptedQoS, const bool retainAsPublished, Properties * properties)
    {
        if (_topic == nullptr)
            return ErrorType::BadParameter;

        // Create the subscribe topic here
        Protocol::MQTT::V5::SubscribeTopic topic(_topic, retainHandling, retainAsPublished, !withAutoFeedBack, maxAcceptedQoS, true);
        // Then proceed to subscribing
        return subscribe(topic, properties);
    }

    MQTTv5::ErrorType MQTTv5::subscribe(SubscribeTopic & topics, Properties * properties)
    {
        ScopedLock scope(impl->lock);
        if (!impl->isOpen()) return ErrorType::NotConnected;
        // If we are interrupting while receiving a packet, let's stop before make any more damage
        if (impl->getLastPacketType() != Protocol::MQTT::V5::RESERVED)
            return ErrorType::TranscientPacket;

        Protocol::MQTT::V5::ControlPacket<Protocol::MQTT::V5::SUBSCRIBE> packet;
        // Capture properties (to avoid copying them)
        packet.props.capture(properties);

#if MQTTAvoidValidation != 1
        if (!packet.props.checkPropertiesFor(Protocol::MQTT::V5::SUBSCRIBE))
            return ErrorType::BadProperties;
#endif


        packet.fixedVariableHeader.packetID = impl->allocatePacketID();
        packet.payload.topics = &topics;

        // Then send the packet
        if (ErrorType ret = prepareSAR(packet))
            return ret;

        // Then extract the packet type
        Protocol::MQTT::V5::ControlPacketType type = impl->getLastPacketType();
        if (type == Protocol::MQTT::V5::SUBACK)
        {
            Protocol::MQTT::V5::ROSubACKPacket rpacket;
            int ret = impl->extractControlPacket(type, rpacket);
            if (ret <= 0) return ErrorType::TranscientPacket;

            if (rpacket.fixedVariableHeader.packetID != packet.fixedVariableHeader.packetID)
                return ErrorType::TranscientPacket;

            // Then check reason codes
            SubscribeTopic * topic = packet.payload.topics;
            uint32 count = topic->count();
            if (!rpacket.payload.data || rpacket.payload.size < count)
                return ReasonCodes::ProtocolError;
            for (uint32 i = 0; i < count; i++)
                if (rpacket.payload.data[i] >= ReasonCodes::UnspecifiedError)
                    return (ReasonCodes)rpacket.payload.data[i];

            return ErrorType::Success;
        }

        return MQTTv5::ErrorType::NetworkError;
    }

#if MQTTUseUnsubscribe == 1
    MQTTv5::ErrorType MQTTv5::unsubscribe(UnsubscribeTopic & topics, Properties * properties)
    {
        ScopedLock scope(impl->lock);
        if (!impl->isOpen()) return ErrorType::NotConnected;
        // If we are interrupting while receiving a packet, let's stop before make any more damage
        if (impl->getLastPacketType() != Protocol::MQTT::V5::RESERVED)
            return ErrorType::TranscientPacket;
        // If we are currently unsubscribing and still waiting for ACK, don't accept any other unsubscription
        if (impl->unsubscribeId)
            return ErrorType::TranscientPacket;

        Protocol::MQTT::V5::ControlPacket<Protocol::MQTT::V5::UNSUBSCRIBE> packet;
        // Capture properties (to avoid copying them)
        packet.props.capture(properties);

#if MQTTAvoidValidation != 1
        if (!packet.props.checkPropertiesFor(Protocol::MQTT::V5::UNSUBSCRIBE))
            return ErrorType::BadProperties;
#endif


        packet.fixedVariableHeader.packetID = impl->allocatePacketID();
        impl->unsubscribeId = packet.fixedVariableHeader.packetID;
        packet.payload.topics = &topics;

        // Then send the packet
        if (ErrorType ret = prepareSAR(packet))
            return ret;

        // Unsubscribe answer will be fetched later on, use getUnsubscribeResult if you are interested in that
        return ErrorType::Success;
    }

    MQTTv5::ErrorType MQTTv5::getUnsubscribeResult()
    {
        ScopedLock scope(impl->lock);
        if (!impl->isOpen()) return ErrorType::NotConnected;
        if (impl->unsubscribeId) return ErrorType::WaitingForResult;
        ErrorType ret = impl->lastUnsubscribeError;

        impl->lastUnsubscribeError = ErrorType::WaitingForResult;
        return ret;
    }
#endif

    // Enter publish cycle
    MQTTv5::ErrorType MQTTv5::enterPublishCycle(Protocol::MQTT::V5::ControlPacketSerializableImpl & publishPacket, bool sending)
    {
        // The type to answer with depends on the QoS value
        uint8 QoS = ((Protocol::MQTT::V5::FixedHeaderType<Protocol::MQTT::V5::PUBLISH, 0>&)publishPacket.header).getQoS();
        uint16 packetID = ((Protocol::MQTT::V5::FixedField<Protocol::MQTT::V5::PUBLISH> &)publishPacket.fixedVariableHeader).packetID;

        static Protocol::MQTT::V5::ControlPacketType nexts[3] = { Protocol::MQTT::V5::RESERVED, Protocol::MQTT::V5::PUBACK, Protocol::MQTT::V5::PUBREC };
        Protocol::MQTT::V5::ControlPacketType next = nexts[QoS];

        /* The state machine is like this:
                    SEND                         RECV
                   [ PUB ] => Send
                - - - - - - - - - - -
                                Receive [ PKT ]
                PKT == ACK ?                     PKT == ACK ?
                / Yes      \ No (REC)            / Yes      \ No (REC)
               Assert ID  Assert ID           Send ACK      Send REC
               |            |                    |      - - - - - - - - -
               Stop       [ REL ] => Send       Stop     Receive [REL]
                        - - - - - - - - - -                   |
                          Receive [COMP]                 Send [COMP]
                            |                                 |
                          Assert ID                         Stop
                            |
                           Stop
        */

        if (sending)
        {
            if (ErrorType ret = prepareSAR(publishPacket, next != Protocol::MQTT::V5::RESERVED))
                return ret;
            // Receive packet so we are at the same position in the state machine above
        }

        while (next != Protocol::MQTT::V5::RESERVED)
        {
            if (sending)
            {   // Skip this if received a Publish packet since it's not the same format
                Protocol::MQTT::V5::PublishReplyPacket reply(next);

                Protocol::MQTT::V5::ControlPacketType type = impl->getLastPacketType();
                if (type != next) return Protocol::MQTT::V5::ProtocolError;

                int ret = impl->extractControlPacket(next, reply);
                if (ret == -3) return ErrorType::TranscientPacket;
                if (ret <= 0) return ErrorType::NetworkError;

                // Ensure it's matching the packet ID
                if (reply.fixedVariableHeader.packetID != packetID)
                    // Could be a protocol error, but this will be checked in the next call to eventLoop
                    return ErrorType::TranscientPacket;

                // Compute the expected next packet
                next = Protocol::MQTT::Common::Helper::getNextPacketType(next);
                if (next == Protocol::MQTT::V5::RESERVED)
                    return ErrorType::Success;
            } else sending = true;

            // Check if we need to send something
            Protocol::MQTT::V5::PublishReplyPacket answer(next);
            answer.fixedVariableHeader.packetID = packetID;
            next = Protocol::MQTT::Common::Helper::getNextPacketType(next);
            if (ErrorType err = prepareSAR(answer, next != Protocol::MQTT::V5::RESERVED))
                return err;
        }
        return ErrorType::Success;
    }

    // Publish to a topic.
    MQTTv5::ErrorType MQTTv5::publish(const char * topic, const uint8 * payload, const uint32 payloadLength, const bool retain, const QoSDelivery QoS, const uint16 packetIdentifier, Properties * properties)
    {
        if (topic == nullptr)
            return ErrorType::BadParameter;

        ScopedLock scope(impl->lock);
        if (!impl->isOpen()) return ErrorType::NotConnected;
        // If we are interrupting while receiving a packet, let's stop before make any more damage
        if (impl->getLastPacketType() != Protocol::MQTT::V5::RESERVED)
            return ErrorType::TranscientPacket;

        Protocol::MQTT::V5::PublishPacket packet;
        // Capture properties (to avoid copying them)
        packet.props.capture(properties);

#if MQTTAvoidValidation != 1
        if (!packet.props.checkPropertiesFor(Protocol::MQTT::V5::PUBLISH))
            return ErrorType::BadProperties;
#endif

        // Create header now
        bool withAnswer = QoS != QoSDelivery::AtMostOne;
        packet.header.setRetain(retain);
        packet.header.setQoS((uint8)QoS);
        packet.header.setDup(false); // At first, it's not a duplicate message
        packet.fixedVariableHeader.packetID = withAnswer ? impl->allocatePacketID() : 0; // Only if QoS is not 0
        packet.fixedVariableHeader.topicName = topic;
        packet.payload.setExpectedPacketSize(payloadLength);
        packet.payload.readFrom(payload, payloadLength);

        return enterPublishCycle(packet, true);
    }

    // The client event loop you must call regularly.
    MQTTv5::ErrorType MQTTv5::eventLoop()
    {
        ScopedLock scope(impl->lock);
        if (!impl->isOpen()) return ErrorType::NotConnected;

        // Check if we have a packet ready for reading now
        Protocol::MQTT::Common::ControlPacketType type = impl->getLastPacketType();
        if (type == Protocol::MQTT::V5::RESERVED)
        {
            // Check if we need to ping the server
            if (impl->shouldPing())
            {
                // Create a Ping request packet and send it
                Protocol::MQTT::V5::PingReqPacket packet;
                if (ErrorType ret = prepareSAR(packet, false))
                    return ret;
            }
            // Check the server for any packet...
            int ret = impl->receiveControlPacket(true);
            if (ret == 0)
            {
                impl->close();
                return ErrorType::NotConnected;
            }
            // No answer in time, it's not an error here
            if (ret == -2) return ErrorType::Success;
            if (ret < 0)   return ErrorType::NetworkError;
            // Ok, now we have a packet read it
            type = impl->getLastPacketType();
        }



        switch (type)
        {
        case Protocol::MQTT::V5::PINGRESP: break; // We ignore ping response
        case Protocol::MQTT::V5::DISCONNECT: impl->close(); return ErrorType::NotConnected; // No work to perform upon server sending disconnect
        case Protocol::MQTT::V5::PUBLISH:
        {
            Protocol::MQTT::V5::ROPublishPacket packet;
            int ret = impl->extractControlPacket(type, packet);
            if (ret == 0) { impl->close(); return ErrorType::NotConnected; }
            if (ret < 0) return ErrorType::NetworkError;
            impl->cb->messageReceived(packet.fixedVariableHeader.topicName, DynamicBinDataView(packet.payload.size, packet.payload.data), packet.fixedVariableHeader.packetID, packet.props);
            return enterPublishCycle(packet, false);
        }
#if MQTTUseUnsubscribe == 1
        case Protocol::MQTT::V5::UNSUBACK:
        {
            Protocol::MQTT::V5::ROUnsubACKPacket rpacket;
            int ret = impl->extractControlPacket(type, rpacket);
            if (ret == 0) { impl->close(); return ErrorType::NotConnected; }
            if (ret < 0) return ErrorType::NetworkError;

            if (rpacket.fixedVariableHeader.packetID != impl->unsubscribeId)
                return ErrorType::NetworkError;

            // Then check reason codes
            uint32 count = rpacket.payload.size;
            if (!rpacket.payload.data)
                impl->lastUnsubscribeError = (ErrorType::Type)ReasonCodes::ProtocolError;
            for (uint32 i = 0; i < count; i++)
                if (rpacket.payload.data[i] >= ReasonCodes::UnspecifiedError)
                    impl->lastUnsubscribeError = (ErrorType::Type)rpacket.payload.data[i];

            // Done processing
            impl->unsubscribeId = 0;
        }
#endif
#if MQTTUseAuth == 1
        case Protocol::MQTT::V5::AUTH:
        {
            return impl->handleAuth();
        }
#endif

        default: // Ignore all other packets currently
            break;
        }

        impl->resetPacketReceivingState();
        return ErrorType::Success;
    }

    // Disconnect from the server
    MQTTv5::ErrorType MQTTv5::disconnect(const ReasonCodes code, Properties * properties)
    {
        if (code != ReasonCodes::NormalDisconnection && code != ReasonCodes::DisconnectWithWillMessage && code < ReasonCodes::UnspecifiedError)
            return ErrorType::BadParameter;


        ScopedLock scope(impl->lock);
        if (!impl->isOpen()) return ErrorType::Success;

        Protocol::MQTT::V5::ControlPacket<Protocol::MQTT::V5::DISCONNECT> packet;
        packet.fixedVariableHeader.reasonCode = code;
        // Capture properties (to avoid copying them)
        packet.props.capture(properties);

#if MQTTAvoidValidation != 1
        if (!packet.props.checkPropertiesFor(Protocol::MQTT::V5::DISCONNECT))
            return ErrorType::BadProperties;
#endif

        if (ErrorType ret = prepareSAR(packet, false))
            return ret;

        // There is no need to wait for ACK for a disconnect
        impl->close();
        return ErrorType::Success;
    }

    void MQTTv5::setDefaultTimeout(const uint32 timeoutMs)
    {
        impl->setTimeout(timeoutMs);
    }

}}
