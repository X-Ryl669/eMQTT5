#ifndef hpp_CPP_Socket_CPP_hpp
#define hpp_CPP_Socket_CPP_hpp

// We need address declarations
#include "Address.hpp"

/** Network specific code, like socket classes declaration and others */
namespace Network
{
    /** The socket namespace contains the socket definitions */
    namespace Socket
    {
        /** Shortcut to the base address interface */
        typedef Address::BaseAddress Address;

        // Forward declare the monitoring pool
        struct MonitoringPool;

        /** They are many types of socket that can be used to communicate.
            The base socket class typically abstract a common interface for such sockets */
        struct BaseSocket
        {
            // Type definition and enumeration
        public:
            /** The socket type (used for creation) */
            enum Type
            {
                Datagram    =   1,  //!< Datagram sockets are used for connection-less setup
                Stream      =   2,  //!< Stream sockets are used for connection based setup
                Raw         =   3,  //!< Raw sockets are used when forging code
                ICMP        =   4,  //!< ICMP sockets are used for ICMP

                Unknown     =   0,  //!< Don't use this, this is an error
            };
            /** The socket options */
            enum Option
            {
                Blocking            =   1,  //!< Set the blocking mode, param is 0 or 1
                ReceiveBufferSize   =   2,  //!< The receive buffer size, param is in bytes
                SendBufferSize      =   3,  //!< The send buffer size, param is in bytes
                ReuseAddress        =   4,  //!< Allow reusing the address, param is 0 or 1
                LingerOnClose       =   5,  //!< When closing, allow sending remaining data (and block while lingering), param is 0 or 1
                Broadcast           =   6,  //!< When datagram based, send datagram to broadcast address, param is 0 or 1
                SendTimeout         =   7,  //!< The send timeout, in millisecond (negative for infinite)
                ReceiveTimeout      =   8,  //!< The receive timeout, in millisecond (negative for infinite)
                RendezVous          =   9,  //!< Enable rendez vous mode, allowing NAT passthrough, param is 0 or 1
                Cork                =  10,  //!< Prevent sending data until we remove the option
                NoDelay             =  11,  //!< Disable Nagle's algorithm
                TCPMaxSeg           =  12,  //!< The TCP maximum segment size (could be asked for or set)
                NoSigPipe           =  13,  //!< Prevent sending a SIGPIPE message if the socket is closed on the remote side
                Descriptor          =  99,  //!< Might return the socket file descriptor on some platform. You don't need this usually.
            };
            /** The possible error code */
            enum Error
            {
                Success             =   0,  //!< The operation succeeded
                InProgress          =   1,  //!< A non blocking socket is doing its work, so you can select it
                Refused             =   -1, //!< Connection refused (on the remote end)
                StillInUse          =   -2, //!< The address is still used
                BadCertificate      =   -3, //!< A secure SSL connection was attempted but the certificate was refused
                OtherError          =   -4, //!< The error isn't specified or useless to understand
                BadArgument         =   -5, //!< An argument passed to the method is not understood / accepted
            };
            /** The socket state (used internally to avoid re-doing stuff) */
            enum State
            {
                Unset       =   0,  //!< The socket is not set yet
                Opened      =   1,  //!< The socket is opened
                Bound       =   2,  //!< The socket is bound to a port and interface
                Listening   =   3,  //!< The socket is listening on the bound port
                Connecting  =   4,  //!< The socket is connecting to a remote port
                Connected   =   5,  //!< The socket is connected to a remote port (this is only valid for client socket)
            };

            /** The asynchronous sendDataAndFile callback declaration */
            struct SDAFCallback
            {
                /** The callback method that's called on sent completion (from another thread)
                    You can delete the socket if you want, it's not used anymore after this call
                    @warning This is called from an unknown thread, so you must make sure about correctly protecting your object */
                virtual void finishedSending(BaseSocket * socket, const uint64 sentSize) = 0;
                virtual ~SDAFCallback() {};
            };

            // Our child interface
        protected:
            /** Open a socket of the specified type */
            virtual bool open(const Type type) = 0;
            /** Close the socket */
            virtual bool close() = 0;
            /** Connection done.
                This is called when the connection to a remote side is established.
                Most socket will ignore this event, but SSL_TLS socket need to be informed when the socket actually connected */
            virtual Error connectionDone() { return Success; }
            /** The last error done */
            mutable Error lastError;

            // Public interface
        public:
            /** Bind a socket on the given address
                @param address          The address to bind on. In case the host address is invalid, and allowBroadcast is false, the socket is bound on all interfaces
                @param allowBroadcast   If true, a broadcast address is allowed to be bound
                @return The translated error for the operation */
            virtual Error bind(const Address & address, const bool allowBroadcast = false) = 0;
            /** Bind a (future listening) socket on all the network interface, with the given port.
                This is just a shortcut to the initial bind method, but it's easier to remember.
                @param port         The port to bind on
                @param broadcast    If true, the socket is using a broadcast address.
                @sa bind */
            virtual Error bindOnAllInterfaces(const uint16 port, const bool broadcast = false) { return bind(Network::Address::IPV4(broadcast ? ~0 : 0, port), broadcast); }
            /** Bind on a multicast group.
                This is used to join a multicast group identified by a multicast IP address.
                @param multicastAddress The group address to join (if any port specified, it's used locally, set to 0 to leave group)
                @param interfaceAddress If specified and valid, bind on the given interface. It's not owned.
                @return The translated error for the operation */
            virtual Error bindOnMulticast(const Address &, const Address * = 0) { return OtherError; }
            /** Connect a socket to the given address */
            virtual Error connect(const Address & address) = 0;
            /** Accept a connection (and fill the address on success) */
            virtual BaseSocket * accept(Address *& address) = 0;
            /** Listen on such socket */
            virtual Error listen(const int maxAllowedConnection = 5) = 0;
            /** Receive data on the socket
                @param buffer       The buffer to store data from
                @param bufferSize   The buffer size in bytes
                @param flags        Any optional flags that's platform specific (checkout the OS's recv function for supported values)
                @return -1 on error, the number of bytes received or 0 if the connection was closed */
            virtual int receive(char * buffer, const int bufferSize, const int flags) const = 0;
            /** Receive data from the socket reliably.
                This waits for the socket to become readable, then read as much as possible in loop until the timeout expires or the buffer is read.
                @warning This operation can take a while, depending on the link speed
                @param buffer      The buffer to read from on the other side
                @param bufferSize  The buffer's size in bytes
                @param timeout     The time to wait before timing out, in milliseconds.
                @return -1 on error, if this method returns less than the asked size, the connection was closed or the timeout expired. */
            virtual int receiveReliably(char * buffer, const int bufferSize, const Time::TimeOut & timeout = DefaultTimeOut) const;
            /** Receive data on the datagram socket.
                This might not be supported on every socket class.
                @param buffer       The buffer to store data from
                @param bufferSize   The buffer size in bytes
                @param from         On input, if the pointer points to valid data, it's deleted. On output, it contains a new allocated address you must delete.
                @return -1 on error, the number of bytes received or 0 if the connection was closed */
            virtual int receiveFrom(char * buffer, const int bufferSize, Address *& from) const = 0;
            /** Send data on the socket
                @return -1 on error, and 0 if the connection was closed */
            virtual int send(const char * buffer, const int bufferSize, const int flags) const = 0;
            /** Send data on the datagram socket.
                This might not be supported on every socket class. */
            virtual int sendTo(const char * buffer, const int bufferSize, const Address & to) const = 0;
            /** Send data on the socket reliably.
                This waits for the socket to become writable, then write as much as possible in loop until the timeout expires or the buffer is sent.
                @param buffer      The buffer to send on the other side
                @param bufferSize  The buffer's size in bytes
                @param splitSize   If not 0, this split the buffer by sending it in block of that size.
                @param timeout     The time to wait before timing out, in milliseconds.
                @return -1 on error, if this method returns less than the asked size, the connection was closed or the timeout expired. */
            virtual int sendReliably(const char * buffer, const int bufferSize, const unsigned int splitSize = 0, const Time::TimeOut & timeout = DefaultTimeOut) const;
            /** This method is used to shorcut the split size parameter and avoid a timeout to splitSize implicit conversion when used improperly */
            inline int sendReliably(const char * buffer, const int bufferSize, const Time::TimeOut & timeout) const { return sendReliably(buffer, bufferSize, 0, timeout); }
            /** Send data and file asynchronously on the socket.
                Not all sockets support this feature, and is emulated with a thread if not supported.
                @param  prefixBuffer    If set, these data are sent before the file is sent.
                @param  prefixLength    The prefix buffer length
                @param  filePath        The file path to send
                @param  offset          The file offset to start with
                @param  size            The file's size to send (can be 0 to send whole file)
                @param  suffixBuffer    If set, these data are sent after the file is sent.
                @param  suffixLength    The suffix buffer length
                @param  callback        When all the data are sent or an error occurred, this callback is called. It must still exist until transfer completed.
                @return true if the operation can start, false otherwise (max number of AIO file reached, or unsupported method)
                @warning You mustn't do anything with the socket while the operation is pending */
            virtual bool sendDataAndFile(const char * prefixBuffer, const int prefixLength, const String & filePath, const uint64 offset, const uint64 size,
                                            const char * suffixBuffer, const int suffixLength, SDAFCallback & callback) = 0;
            /** Send data from multiple buffer at once.
                This can be implemented efficiently in some OS, so the best method is used whenever applicable.
                The default implementation iterate over the given buffers
                @param  buffers         An array of pointers to the buffers to send
                @param  buffersSize     An array of buffer sizes in bytes
                @param  buffersCount    The number of buffers to send
                @param  flags           Any sending flags
                @return -1 on error, the number of bytes sent */
            virtual int sendBuffers(const char ** buffers, const int * buffersSize, const int buffersCount, const int flags = 0) const
            {
                int ret = 0;
                if (buffers == 0 || buffersSize == 0) return -1;
                for (int i = 0; i < buffersCount; i++)
                {
                    int local = 0;
                    while (local < buffersSize[i])
                    {
                       int loc = send(&buffers[i][local], buffersSize[i] - local, flags);
                       if (loc == 0) return ret;
                       if (loc < 0) return -1;
                       local += loc;
                    }
                    ret += local;
                }
                return ret;
            }
            /** Send data from multiple buffer at once to the specified address (this only works for datagram sockets).
                This can be implemented efficiently in some OS, so the best method is used whenever applicable.
                The default implementation iterate over the given buffers (this works for TCP not for UDP
                since multiple datagram will be sent)
                @param  buffers         An array of pointers to the buffers to send
                @param  buffersSize     An array of buffer sizes in bytes
                @param  buffersCount    The number of buffers to send
                @param  to              The address to send to
                @return -1 on error, the number of bytes sent */
            virtual int sendBuffersTo(const char ** buffers, const int * buffersSize, const int buffersCount, const Address & to) const
            {
                int ret = 0;
                if (buffers == 0 || buffersSize == 0) return -1;
                for (int i = 0; i < buffersCount; i++)
                {
                    int local = 0;
                    while (local < buffersSize[i])
                    {
                       int loc = sendTo(&buffers[i][local], buffersSize[i] - local, to);
                       if (loc == 0) return ret;
                       if (loc < 0) return -1;
                       local += loc;
                    }
                    ret += local;
                }
                return ret;
            }
            /** Cancel an asynchronous sending */
            virtual bool cancelAsyncSend() = 0;
            /** Set the socket option */
            virtual bool setOption(const Option option, const int value) = 0;
            /** Get the socket option */
            virtual bool getOption(const Option option, int & value) = 0;
            /** Select on this socket.
                @param reading When true, the select return true as soon as the socket has read data available
                @param writing When true, the select return true as soon as the socket is ready to be written to
                @param timeout The timeout in millisecond to wait for before returning (negative for infinite time)
                @return true if the socket match the queried requirement, false otherwise */
            virtual bool select(const bool reading, const bool writing, const Time::TimeOut & timeout = DefaultTimeOut) const = 0;
            /** Append this socket to a monitoring pool (so it's possible to select over multiple socket later on)
                @param pool     If set, this socket is appended to this pool. You can use a null pointer to create a new pool.
                @return A MonitoringPool object that can later be used to select over multiple socket (or 0 on error). You must delete this object */
            virtual MonitoringPool * appendToMonitoringPool(MonitoringPool * pool) = 0;
            /** Get the socket type  */
            virtual Type getType() const = 0;
            /** Get the socket class instantiation number. Poor man RTTI */
            virtual int getTypeID() const = 0;
            /** Get the socket state  */
            virtual State getState() const = 0;
            /** Get the peer name for a connected socket
                @return a pointer to an Address you must delete on success, or 0 on error */
            virtual Address * getPeerName() const = 0;
            /** Get the bound address for a connected socket
                @return a pointer to an Address you must delete on success, or 0 on error */
            virtual Address * getBoundAddress() const = 0;
            /** Get the private field.
                This is used to store an unknown data per socket.
                It is set to 0 at construction.

                @return a reference to a pointer you can set to whatever struct you want.
                @warning you must manage this field allocation and destruction. */
            virtual void * & getPrivateField() = 0;
            /** Get the last failure reason */
            virtual Error   getLastError() const { return lastError; }

            BaseSocket() : lastError(Success) {}
            virtual ~BaseSocket() { }

            /** Check if a return code is an error or not.
                This only returns true on real definitive error (ie. not on pending connection or temporary error) */
            bool isError(const Error error) const { return (int)error < 0; }

            // Helpers
        protected:
            /** Set the state and return success */
            virtual Error setState(const State newState) = 0;
            /** Set the last error by checking the system dependant error.
                This is read only as the error is also tracked in read only operation */
            Error setError(const Error error = OtherError) const;
            /** Set the error from the usual int based error code. */
            int setError(int errorCode) const;

            // Allow some access to our other pseudo-child class
        public:
            /** The Local class is allowed to open and close socket by its own */
            friend class Local;
        };

        /** Berkeley socket use Berkeley's implementation
            Typically, the one present in Linux and other Posix system */
        struct BerkeleySocket : public BaseSocket
        {
            // Type definition and enumeration
        public:
#ifdef _WIN32
            /** The usual error codes */
            enum ErrorFunc { SocketError = SOCKET_ERROR };
            /** The socket type to use internally */
            typedef SOCKET      sock_t;
#else
            /** The usual error codes */
            enum ErrorFunc { SocketError = -1 };
            /** The socket type to use internally */
            typedef int         sock_t;
#endif

            // Members
        protected:
            /** The actual socket descriptor */
            sock_t      descriptor;
            /** The socket type */
            Type        type;
            /** The socket state */
            State       state;
            /** The private field */
            void *      priv;
#ifdef _LINUX
            /** Additional sending options (for platform requiring it) */
            ZeroInit<int> sendOptions;
#endif

            // BaseSocket Interface
        protected:
            /** Open a socket of the specified type */
            virtual bool open(const Type type);
            /** Close the socket */
            virtual bool close();

        public:
            /** Bind a socket on the given address */
            virtual Error bind(const Address & address, const bool allowBroadcast = false);
            /** Bind on a multicast group.
                This is used to join a multicast group identified by a multicast IP address.
                @param multicastAddress The group address to join (if any port specified, it's used locally)
                @param interfaceAddress If specified and valid, bind on the given interface. It's not owned.
                @return The translated error for the operation */
            virtual Error bindOnMulticast(const Address & multicastAddress, const Address * interfaceAddress = 0);
            /** Bind on a multicast interface.
                Unlike the bindOnMulticast method, this only set the socket to multicast on a specific interface.
                It does not join a group.
                @param interfaceAddress     The interface address
                @return true on success */
            virtual bool bindMulticastInterface(const Address & interfaceAddress);
            /** Connect a socket to the given address */
            virtual Error connect(const Address & address);
            /** Accept a connection (and fill the address on success) */
            virtual BaseSocket * accept(Address *& address);
            /** Listen on such socket */
            virtual Error listen(const int maxAllowedConnection = 5);
            /** Receive data on the socket */
            virtual int receive(char * buffer, const int bufferSize, const int flags) const;
            /** Receive data on the datagram socket.
                This might not be supported on every socket class.
                @warning You must delete the returned address */
            virtual int receiveFrom(char * buffer, const int bufferSize, Address *& from) const;
            /** Send data on the datagram socket.
                This might not be supported on every socket class. */
            virtual int sendTo(const char * buffer, const int bufferSize, const Address & to) const;
            /** Send data on the socket */
            virtual int send(const char * buffer, const int bufferSize, const int flags) const;
            /** Send data and file asynchronously on the socket.
                Not all sockets support this feature, and is emulated with a thread if not supported.
                @param  prefixBuffer    If set, these data are sent before the file is sent.
                @param  prefixLength    The prefix buffer length
                @param  filePath        The file path to send
                @param  offset          The file offset to start with
                @param  size            The file's size to send (can be 0 to send whole file)
                @param  suffixBuffer    If set, these data are sent after the file is sent.
                @param  suffixLength    The suffix buffer length
                @param  callback        When all the data are sent or an error occurred, this callback is called. It must still exist until transfer completed.
                @return true if the operation can start, false otherwise (max number of AIO file reached, or unsupported method) */
            virtual bool sendDataAndFile(const char * prefixBuffer, const int prefixLength, const String & filePath, const uint64 offset, const uint64 size,
                                            const char * suffixBuffer, const int suffixLength, SDAFCallback & callback);
            /** Cancel an asynchronous sending */
            virtual bool cancelAsyncSend();
            /** Send data from multiple buffer at once.
                This can be implemented efficiently in some OS, so the best method is used whenever applicable.
                The default implementation iterate over the given buffers
                @param  buffers         An array of pointers to the buffers to send
                @param  buffersSize     An array of buffer sizes in bytes
                @param  buffersCount    The number of buffers to send
                @param  flags           Any sending flags
                @return -1 on error, the number of bytes sent */
            virtual int sendBuffers(const char ** buffers, const int * buffersSize, const int buffersCount, const int flags = 0) const;
            /** Send data from multiple buffer at once to the specified address (this only works for datagram sockets).
                This can be implemented efficiently in some OS, so the best method is used whenever applicable.
                The default implementation iterate over the given buffers (this works for TCP not for UDP
                since multiple datagram will be sent)
                @param  buffers         An array of pointers to the buffers to send
                @param  buffersSize     An array of buffer sizes in bytes
                @param  buffersCount    The number of buffers to send
                @param  to              The address to send to
                @return -1 on error, the number of bytes sent */
            virtual int sendBuffersTo(const char ** buffers, const int * buffersSize, const int buffersCount, const Address & to) const;
            /** Set the socket option */
            virtual bool setOption(const Option option, const int value);
            /** Get the socket option */
            virtual bool getOption(const Option option, int & value);
            /** Select on this socket.
                @param reading When true, the select return true as soon as the socket has read data available
                @param writing When true, the select return true as soon as the socket is ready to be written to
                @param timeout The timeout in millisecond to wait for before returning (negative for infinite time)
                @return true if the socket match the queried requirement, false otherwise */
            virtual bool select(const bool reading, const bool writing, const Time::TimeOut & timeout = DefaultTimeOut) const;
            /** Append this socket to a monitoring pool (so it's possible to select over multiple socket later on)
                @param pool     If set, this socket is appended to this pool. You can use a null pointer to create a new pool.
                @return A MonitoringPool object that can later be used to select over multiple socket (or 0 on error). You must delete this object */
            virtual MonitoringPool * appendToMonitoringPool(MonitoringPool * pool);
            /** Get the socket type  */
            virtual Type getType() const { return type; }
            /** Get the socket class instantiation number */
            virtual int getTypeID() const { return 1; }
            /** Get the socket state  */
            virtual State getState() const { return state; }
            /** Get the peer name for a connected socket
                @return a pointer to an Address you must delete on success, or 0 on error */
            virtual Address * getPeerName() const;
            /** Get the bound address for a connected socket
                @return a pointer to an Address you must delete on success, or 0 on error */
            virtual Address * getBoundAddress() const;
            /** Get the private field.
                This is used to store an unknown data per socket.
                @return a reference to a pointer you can set to whatever struct you want.
                @warning you must manage this field allocation and destruction. */
            virtual void * & getPrivateField() { return priv; }


            // Our interface
        public:
            /** Let the Berkeley monitoring pool access our internal stuff */
            friend class BerkeleyPool;
#ifdef _POSIX
            /** The epoll/kqueue mechanism requires file descriptor */
            friend class FastBerkeleyPool;
#endif
            friend struct WaitingThread;

            /** Open a socket of the specified type (internal) */
            virtual bool open(const bool ipv6);

            // Helpers
        protected:
            /** This constructor is used internally to build the object from the underlying stuff */
            BerkeleySocket(const sock_t desc, const Type type, const State state = Connecting) : descriptor(desc), type(type), state(state == Unset ? (desc == 0 ? Unset : Opened) : state), priv(0) {}
            /** Set the state and return success */
            Error setState(const State newState);

            // Construction and destruction
        public:
            /** Construction */
            BerkeleySocket() : descriptor((sock_t)SocketError), type(Unknown), state(Unset), priv(0) {}
            /** Construct a socket of the given type */
            BerkeleySocket(const Type type) : descriptor((sock_t)SocketError), type(type), state(Unset), priv(0) {}

            /** Our destructor */
            ~BerkeleySocket() { close(); }
        };

        /** The monitoring pool base interface.

            Typically, using a monitoring pool, is as simple as appending socket to monitor to the pool,
            then calling the monitoring method, and then get the results.
            @code
                // Construct a pool
                MonitoringPool & pool = ...;
                // Ok, let's accept multiple clients
                Address * clientAddress = 0;
                pool.appendSocket(server.accept(clientAddress));
                printf("Received client %s\n", (const char*)clientAddress.asText());

                // Then monitor when there is data ready for the client
                if (pool.isReadPossible())
                {
                    int index = pool.getNextReadySocket();
                    while (index >= 0)
                    {
                        // Your reading method is here
                        pool.getReadyAt(index)->receive(buffer, bufferSize, 0);
                        // Parse input, and fill output
                        pool.getReadyAt(index)->send(outBuffer, outBufferSize, 0);
                        index = pool.getNextReadySocket(index);
                    }
                }
            @endcode
            @warning A pool never owns the socket passed in unless you specify it in its constructor (see specialization)
            @warning Another common mistake is to monitor for write state of the socket. Most of the time, sockets are writeable,
                     so doing so results in a O(N) behaviour, since you'll scan the entire socket set even if no write is required.
                     The right way to deal with this is to create a new pool only for "sockets that need to write", and select on this pool only.
                     If you need atomic wait on both pool, you can use selectMultiple method
            @warning If you store SSL sockets into this pool you must completely read all the buffers (kernel + SSL) once it says the socket is
                     readable. If you don't do so, you might still have data in the SSL buffer, yet waiting on read will block since it's done at
                     the socket/kernel level. */
        struct MonitoringPool
        {
            /** Append a socket to this pool */
            virtual bool appendSocket(BaseSocket * socket) = 0;
            /** Remove a socket from the pool.
                The socket is deleted only if the pool own the sockets. */
            virtual bool removeSocket(BaseSocket * socket) = 0;
            /** Forget a socket from the pool.
                The socket is not deleted, even if the pool own the sockets. */
            virtual bool forgetSocket(BaseSocket * socket) = 0;
            /** Get the pool size */
            virtual uint32 getSize() const = 0;
            /** Select the pool for at least an element that is ready
                @param reading When true, the select return true as soon as the socket has read data available
                @param writing When true, the select return true as soon as the socket is ready to be written to
                @param timeout The timeout in millisecond to wait for before returning (negative for infinite time)
                @return false on timeout or error, or true if at least one socket in the pool is ready */
            virtual bool select(const bool reading, const bool writing, const Time::TimeOut & timeout = DefaultTimeOut) const = 0;

            /** Check if at least a socket in the pool is ready for reading
                @param timeout  The timeout to wait for in millisecond */
            virtual bool isReadPossible(const Time::TimeOut & timeout = DefaultTimeOut) const = 0;
            /** Check if at least a socket in the pool is ready for writing
                @param timeout  The timeout to wait for in millisecond */
            virtual bool isWritePossible(const Time::TimeOut & timeout = DefaultTimeOut) const = 0;
            /** Check if a socket is connected.
                @warning this put the sockets in non blocking mode, and put them back in blocking mode automatically after this call
                @param timeout  The timeout to wait for in millisecond */
            virtual bool isConnected(const Time::TimeOut & timeout = DefaultTimeOut) = 0;

            /** Check which socket was ready in the given pool
                @param index    Start by this index when searching (start by -1)
                @return index of the next ready socket (use getReadyAt() to get the socket), or -1 if none are ready */
            virtual int getNextReadySocket(const int index = -1) const = 0;
            /** Get the socket at the given position */
            virtual BaseSocket * operator[] (const int index) = 0;
            /** Get the socket at the given position */
            virtual const BaseSocket * operator[] (const int index) const = 0;
            /** Get the ready socket at the given position
                @param index    The socket index as returned by getNextReadySocket() (this is not necessarly the socket's index in the pool)
                @param writing  If provided, will be set to true if the socket is ready for writing
                @return A pointer on a socket that's ready for operation or 0 on error */
            virtual BaseSocket * getReadyAt(const int index, bool * writing = 0) = 0;
            /** Get the index of the given socket in the pool
                @return getSize() if not found, or the index in the pool */
            virtual uint32 indexOf(BaseSocket * socket) const = 0;
            /** Check if we already have the given socket in the pool */
            virtual bool haveSocket(BaseSocket * socket) const = 0;

            /** Clear the pool from all its sockets */
            virtual void clearPool() = 0;

            /** Get the pool type. This is used as a poor man RTTI */
            virtual int getTypeID() const = 0;
            /** Create an empty pool similar to this one you must delete */
            virtual MonitoringPool * createEmpty(const bool own = false) const = 0;

            /** Select our pool for reading, and the other pool for writing.
                This is equivalent to (isReadPossible() || other.isWritePossible()), simultaneously.
                @warning Use this method only if you already know how to use standard multiple pool select.
                @param other    The other pool to select for writing
                @param timeout  The timeout in millisecond to wait for before returning (negative for infinite time)
                @return 0 if no socket are ready or timed-out, 1 if the our pool got socket(s) ready, 2 if the other pool got socket(s) ready (or 3 if both are ready) */
            virtual int selectMultiple(MonitoringPool * other, const Time::TimeOut & timeout = DefaultTimeOut) const = 0;

            /** Required virtual destructor */
            virtual ~MonitoringPool() {}
        };

        /** The monitoring pool for Berkeley sockets.
            @sa MonitoringPool */
        class BerkeleyPool : public MonitoringPool
        {
            // Type definition and enumeration
        public:
            /** The maximum number of socket in the pool */
            enum { MaxQueueLen = FD_SETSIZE };

            // Members
        private:
            /** The socket pool */
            BerkeleySocket **   pool;
            /** The pool size */
            uint32              size;
            /** The FD set */
            mutable fd_set      rset;
            /** The FD set */
            mutable fd_set      wset;
            /** Are we owning the sockets ? */
            bool                own;

            // Monitoring pool interface
        public:
            /** Append a socket to this pool */
            virtual bool    appendSocket(BaseSocket * socket);
            /** Remove a socket from the pool */
            virtual bool    removeSocket(BaseSocket * socket);
            /** Forget a socket from the pool.
                The socket is not deleted, even if the pool own the sockets. */
            virtual bool    forgetSocket(BaseSocket * socket);
            /** Get the pool size */
            virtual uint32  getSize() const;

            /** Get the pool type. This is used as a poor man RTTI */
            virtual int getTypeID() const { return 1; }
            /** Create an empty pool similar to this one you must delete */
            virtual MonitoringPool * createEmpty(const bool own = false) const { return new BerkeleyPool(own); }

            /** Select the pool for at least an element that is ready
                @param reading When true, the select return true as soon as the socket has read data available
                @param writing When true, the select return true as soon as the socket is ready to be written to
                @param timeout The timeout in millisecond to wait for before returning (negative for infinite time)
                @return false on timeout or error, or true if at least one socket in the pool is ready */
            virtual bool select(const bool reading, const bool writing, const Time::TimeOut & timeout = DefaultTimeOut) const;

            /** Check if at least a socket in the pool is ready for reading */
            virtual bool isReadPossible(const Time::TimeOut & timeout = DefaultTimeOut) const;
            /** Check if at least a socket in the pool is ready for writing */
            virtual bool isWritePossible(const Time::TimeOut & timeout = DefaultTimeOut) const;
            /** Check if a socket is connected.
                @warning this put the sockets in non blocking mode, and put them back in blocking mode automatically after this call */
            virtual bool isConnected(const Time::TimeOut & timeout = DefaultTimeOut);

            /** Check which socket was ready in the given pool
                @param index    Start by this index when searching (start by -1)
                @return index of the next ready socket (use getReadySocketAt() to get the socket), or -1 if none are ready */
            virtual int getNextReadySocket(const int index = -1) const;
            /** Get the socket at the given position */
            virtual BaseSocket * operator[] (const int index);
            /** Get the socket at the given position */
            virtual const BaseSocket * operator[] (const int index) const;
            /** Get the ready socket at the given position
                @param index    The socket index as returned by getNextReadySocket() (this is not necessarly the socket's index in the pool)
                @param writing  If provided, will be set to true if the socket is ready for writing
                @return A pointer on a socket that's ready for operation or 0 on error */
            virtual BaseSocket * getReadyAt(const int index, bool * writing = 0);
            /** Get the index of the given socket in the pool
                @return getSize() if not found, or the index in the pool */
            virtual uint32 indexOf(BaseSocket * socket) const;
            /** Check if we already have the given socket in the pool */
            virtual bool haveSocket(BaseSocket * socket) const;
            /** Clear the pool from all its sockets */
            virtual void clearPool();

            /** Select our for reading for reading, and the other pool for writing.
                This is equivalent to (isReadPossible() || other.isWritePossible()), simultaneously.
                @warning Use this method only if you already know how to use standard multiple pool select.
                @param other    The other pool to select for writing
                @param timeout  The timeout in millisecond to wait for before returning (negative for infinite time)
                @return 0 if no socket are ready or timed-out, 1 if the our pool got socket(s) ready, 2 if the other pool got socket(s) ready */
            virtual int selectMultiple(MonitoringPool * other, const Time::TimeOut & timeout = DefaultTimeOut) const;

            // Construction and destruction
        public:
            BerkeleyPool(const bool own = false) : pool(0), size(0), own(own) { }
            ~BerkeleyPool();
        };

#ifndef _POSIX
        /** There is no fast Berkeley pool for windows */
        typedef BerkeleyPool FastBerkeleyPool;
#else
        /** The monitoring pool for Berkeley sockets.
            @sa MonitoringPool */
        class FastBerkeleyPool : public MonitoringPool
        {
            // Type definition and enumeration
        public:
            /** The maximum number of socket in the pool */
            enum { MaxQueueLen = 16384 };


            // Members
        private:
            /** The socket pool */
            BerkeleySocket **   pool;
            /** The pool size */
            uint32              size;
            /** The epoll/kqueue FD set for reading */
            int                 rd;
            /** The epoll/kqueue FD set for writing */
            int                 wd;
            /** The epoll/kqueue FD set for both event */
            int                 bd;
            /** The events array to monitor */
            void *              events;
            /** The last request triggered event's count */
            mutable int         triggerCount;

            /** Are we owning the sockets ? */
            bool                own;

            // Monitoring pool interface
        public:
            /** Append a socket to this pool */
            virtual bool    appendSocket(BaseSocket * socket);
            /** Remove a socket from the pool */
            virtual bool    removeSocket(BaseSocket * socket);
            /** Forget a socket from the pool.
                The socket is not deleted, even if the pool own the sockets. */
            virtual bool    forgetSocket(BaseSocket * socket);
            /** Get the pool size */
            virtual uint32  getSize() const;
            /** Get the pool type. This is used as a poor man RTTI */
            virtual int getTypeID() const { return 2; }
            /** Create an empty pool similar to this one you must delete */
            virtual MonitoringPool * createEmpty(const bool own = false) const { return new FastBerkeleyPool(own); }

            /** Select the pool for at least an element that is ready
                @param reading When true, the select return true as soon as the socket has read data available
                @param writing When true, the select return true as soon as the socket is ready to be written to
                @param timeout The timeout in millisecond to wait for before returning (negative for infinite time)
                @return false on timeout or error, or true if at least one socket in the pool is ready */
            virtual bool select(const bool reading, const bool writing, const Time::TimeOut & timeout = DefaultTimeOut) const;

            /** Check if at least a socket in the pool is ready for reading */
            virtual bool isReadPossible(const Time::TimeOut & timeout = DefaultTimeOut) const;
            /** Check if at least a socket in the pool is ready for writing */
            virtual bool isWritePossible(const Time::TimeOut & timeout = DefaultTimeOut) const;
            /** Check if a socket is connected.
                @warning this put the sockets in non blocking mode, and put them back in blocking mode automatically after this call */
            virtual bool isConnected(const Time::TimeOut & timeout = DefaultTimeOut);

            /** Check which socket was ready in the given pool
                @param index    Start by this index when searching (start by -1)
                @return index of the next ready socket (use getReadySocketAt() to get the socket), or -1 if none are ready */
            virtual int getNextReadySocket(const int index = -1) const;
            /** Get the socket at the given position */
            virtual BaseSocket * operator[] (const int index);
            /** Get the socket at the given position */
            virtual const BaseSocket * operator[] (const int index) const;
            /** Get the ready socket at the given position
                @param index    The socket index as returned by getNextReadySocket() (this is not necessarly the socket's index in the pool)
                @param writing  If provided, will be set to true if the socket is ready for writing
                @return A pointer on a socket that's ready for operation or 0 on error */
            virtual BaseSocket * getReadyAt(const int index, bool * writing = 0);
            /** Get the index of the given socket in the pool
                @return getSize() if not found, or the index in the pool */
            virtual uint32 indexOf(BaseSocket * socket) const;
            /** Check if we already have the given socket in the pool */
            virtual bool haveSocket(BaseSocket * socket) const;
            /** Clear the pool from all its sockets */
            virtual void clearPool();

            /** Select our for reading for reading, and the other pool for writing.
                This is equivalent to (isReadPossible() || other.isWritePossible()), simultaneously.
                @warning Use this method only if you already know how to use standard multiple pool select.
                @param other    The other pool to select for writing
                @param timeout  The timeout in millisecond to wait for before returning (negative for infinite time)
                @return 0 if no socket are ready or timed-out, 1 if the our pool got socket(s) ready, 2 if the other pool got socket(s) ready */
            virtual int selectMultiple(MonitoringPool * other, const Time::TimeOut & timeout = DefaultTimeOut) const;

            // Construction and destruction
        public:
            /** Build a pool using epoll or kqueue when available.
                @param own  When set to true, the poll own the socket passed in. */
            FastBerkeleyPool(const bool own = false) : pool(0), size(0), rd(-1), wd(-1), bd(-1), events(0), triggerCount(0), own(own) { }
            ~FastBerkeleyPool();
        };
#endif

    }
}

#endif
