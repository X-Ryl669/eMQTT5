#ifndef hpp_CPP_Address_CPP_hpp
#define hpp_CPP_Address_CPP_hpp

// We need strings
#include "Strings/Strings.hpp"
// We need timeout too
#include "Time/Timeout.hpp"


/** Network specific code, like socket classes declaration and others.

    You'll use code from this namespace to build network aware applications.

    The Address namespace provides class to manipulate network addresses
    (from low level like Network::Address::IPV4, Network::Address::IPV6, to full URL
    parsing class like Network::Address::URL)


    This namespace supports many functions for socket based programming, but also for
    network interface analysis.

    For example, you can detect the private IP of all interfaces, their route, their mask.
    You can detect the interface type (Ethernet / Wifi), the speed (100M / 54M), the Wifi
    connected SSID and key (in Network::Address::IPV4 and Network::Address::IPV6)

    You can also use the UDT library (stream connection over UDP, useful for NAT passthrough)
    transparently (in Network::Socket::UDTSocket)

    One of the main interest of this library, is to abstract the used socket class, and deal
    with socket that would be best suited for the destination.
    This can be done easily with the Network::Socket::Local class.

    You might not want to deal with socket at all, and instead only with connections.
    In that case, see Network::Connection::Datagram and Network::Connection::Stream

    If you intend to monitor multiple sockets (servers, P2P), you will be interested in
    Network::Socket::MonitoringPool (abstraction over select, or epoll when available).

    If you plan to write a server, don't miss Network::Server namespace, you'll get a full
    performance server (best known method to date) for your application by writing a very
    small callback class.

*/
namespace Network
{
    /** The default timeout in millisecond */
    enum { DefaultTimeOut = ::Time::DefaultTimeOut };
    /** The string class we are using */
    typedef Strings::FastString String;

    /** Classes used to manipulate network addresses */
    namespace Address
    {
        /** The base interface for a network device address */
        struct BaseAddress
        {
            // Type definition and enumeration
        public:
            /** The known protocol */
            enum Protocol
            {
                Unknown     =    0, //!< Unknown
                Echo        =    7, //!< Echo (official)
                Discard     =    9, //!< Discard (official)
                DayTime     =   13, //!< DayTime (RFC867)
                FTPData     =   20, //!< FTP data (RFC765)
                FTP         =   21, //!< FTP control (RFC765)
                SSH         =   22, //!< Secure Shell (RFC4251)
                Telnet      =   23, //!< Telnet (RFC854)
                SMTP        =   25, //!< Simple Mail Transfer Protocol (RFC821)
                Time        =   37, //!< Time (RFC868)
                WINS        =   42, //!< Windows Name Server (Microsoft)
                WhoIs       =   43, //!< Who is (RFC3912)
                DNS         =   53, //!< Domain Name Server (RFC920+)
                DHCP        =   67, //!< Dynamic Host Configuration Protocol (RFC2132)
                BootPClient =   68, //!< The bootstrap client DHCP (RFC951)
                TFTP        =   69, //!< Trivial File Transfer Protocol (RFC1350)
                HTTP        =   80, //!< HyperText Transfer Protocol (RFC2616)
                POP3        =  110, //!< Post Office Protocol 3 (RFC1939)
                NNTP        =  119, //!< Network News Transfer Protocol (RFC3977)
                NTP         =  123, //!< Network Time Protocol (RFC1305)
                IMAP        =  143, //!< Internet Message Access Protocol (RFC3501)
                SNMP        =  161, //!< Simple Network Management Protocol (RFC3411)
                IRC         =  194, //!< Internet Relay Chat (RFC1459)
                LDAP        =  389, //!< Lightweight Directory Access Protocol (RFC4510)
                HTTPS       =  443, //!< Hypertext Transfer Protocol over TLS/SSL (RFC2818)
                Samba       =  445, //!< Microsoft-DS SMB file sharing (Microsoft)
                SMTPS       =  465, //!< SMTP over SSL (no RFC)
                DHCP6       =  546, //!< DHCPv6 client (RFC3736)
                RTSP        =  554, //!< Real Time Streaming Protocol (RFC2326)
                NNTPS       =  563, //!< NNTP over TLS/SSL (no RFC)
            };

            /** Get the address as a UTF-8 textual value */
            virtual String asText() const = 0;
            /** Set the address as a UTF-8 textual value
                @return false if the given value can not be parsed */
            virtual bool asText(const String & textualValue) = 0;

            /** Get the port if specified in the address (0 otherwise) */
            virtual uint16 getPort() const = 0;
            /** Set the port if possible in this address
                @return false if the given port isn't available */
            virtual bool setPort(const uint16 port) = 0;

            /** Get the protocol (if specified in this address) */
            virtual Protocol getProtocol() const = 0;
            /** Set the protocol (if specified in this address)
                @return false if the given protocol isn't supported */
            virtual bool setProtocol(const Protocol protocol) = 0;

            /** Check if the protocol likely requires a SSL socket */
            static bool isProtocolUsingSSL(const Protocol protocol) { return protocol == HTTPS || protocol == NNTPS || protocol == SMTPS; }
            /** Check if the protocol likely requires a SSL socket */
            inline bool isProtocolUsingSSL() const { return isProtocolUsingSSL(getProtocol()); }

            /** Check if the address is valid */
            virtual bool isValid() const = 0;
            /** Get the public address (if behind NAT)
                @return 0 for local, unNAT'd address, or a pointer to the public address */
            virtual BaseAddress * getPublicAddress() { return 0; }
            /** Set the public address (if behind NAT)
                @return false if unsupported */
            virtual bool setPublicAddress(BaseAddress *, const int = 0) { return false; }
            /** Get a complete address string (used for debugging mainly) including port and/or protocol if it makes sense */
            virtual String identify() const { return asText() + ":" + getPort(); }

            /** Get a bindable address.
                This method is used to ensure an address is bindable by a socket.
                @warning This method is useless for consumer code, it's used by the socket class if it doesn't understand the initial base address
                @return A base address that can be used to be bound correctly */
            virtual const BaseAddress & getBindableAddress() const = 0;
            /** Get the low level object for this address
                @warning Don't use this, it's not what you think it is */
            virtual const void * getLowLevelObject() const = 0;
            /** Get the low level object size for this address
                @warning Don't use this, it's not what you think it is */
            virtual const int getLowLevelObjectSize() const = 0;

            /** Get type id for this address.
                This is used instead of RTTI */
            virtual int getTypeID() const = 0;
            /** Clone this address
                @return a pointer on a new allocated address you must delete. */
            virtual BaseAddress * clone() const = 0;

            /** Required destructor */
            virtual ~BaseAddress() {}
        };

        /** The IPV4 address is a kind of network address */
        class IPV4 : public BaseAddress
        {
            // Type definition and enumeration
        public:
            /** The local interface type */
            enum Type
            {
                Ethernet10M         =   0x00000001, //!< Ethernet 10Mbits/s
                Ethernet100M        =   0x00000002, //!< Ethernet 100Mbits/s
                Ethernet1G          =   0x00000003, //!< Ethernet 1GBits/s
                EthernetMask        =   0x0000000F, //!< Use a test like "if (type & EthernetMask)" to check for ethernet link

                WIFI11M             =   0x00000010, //!< WIFI 802.11b
                WIFI54M             =   0x00000020, //!< WIFI 802.11g
                WIFI108M            =   0x00000030, //!< WIFI 802.11n draft
                WIFI300M            =   0x00000040, //!< WIFI 802.11n
                WIFIMask            =   0x000000F0, //!< Use a test like "if (type & WIFIMask)" to check for wifi link

                DialUp              =   0x00000100, //!< Dial up modem (likely low bitrate)
                DialUpMask          =   0x00000F00, //!< Use a test like "if (type & DialUpMask)" to check for dial up link

                Firewire400M        =   0x00001000, //!< FireWire (1394 link)
                Firewire800M        =   0x00002000, //!< FireWire (1394 link)
                FirewireMask        =   0x0000F000, //!< Use a test like "if (type & FirewireMask)" to check for firewire link

                Other               =   0x10000000, //!< Another type of interface, unknown to us
            };

            /** The class type (as specified in RFC791) */
            enum Class
            {
                ClassA              = 0x1,    //!< Typically any IPv4 address starting by x with x <= 127 (~2^24 possible addresses)
                ClassB              = 0x2,    //!< Typically any IPv4 address starting by x with 127 < x <= 191 (~2^16 possible addresses)
                ClassC              = 0x3,    //!< Typically any IPv4 address starting by x with 192 < x <= 223 (~2^8 possible addresses)
                ClassD              = 0x4,    //!< Typically any IPv4 address starting by x with 224 < x <= 239 (~2^8 possible addresses)
                ClassE              = 0x5,    //!< Typically any IPv4 address starting by x with 240 < x <= 255 (~2^8 possible addresses)
            };

            /** The special address fields (as specified in RFC3330) */
            enum SpecialAddress
            {
                Zero                =   0x00,   //!< Typically an address set to 0.0.0.0/8 (usually class A)
                Private10           =   0x10,   //!< Typically an address set to 10.0.0.0/8 (usually class A)
                Loopback            =  0x100,   //!< Typically an address set to 127.0.0.0/8 (usually class A)

                ZeroConf            =   0x40,   //!< Typically an address set to 169.254.0.0/16 (usually class B)
                Private172          =   0x30,   //!< Typically an address set to 172.16.0.0/16 up to 172.31.0.0/16 (usually class B)

                Documentation       =   0xA0,   //!< Typically an address set to 192.0.2.0/24 (usually class C)
                IPv6ToIPv4          =   0x60,   //!< Typically an address set to 192.88.99.0/24 (usually class C)
                Private192          =   0x50,   //!< Typically an address set to 192.168.0.0/16 (usually class C)
                NetworkBenchmark    =   0x80,   //!< Typically an address set to 192.18.0.0/15 (usually class C)

                Multicast           =   0xA0,   //!< Typically an address set to 224.0.0.0/4 (usually class D)
                Reserved            =   0xC0,   //!< Typically an address set to 240.0.0.0/4 (usually class E)

                Routable            =   0x20,  //!< A routable address on the internet

                PrivateMask         =   0x10,   //!< Use a test like "if (addressType & PrivateMask)" to check for private link
                UsableMask          =   0xFF,   //!< Use a test like "if (addressType & UsableMask)" to check for usable link
            };

            // Members
        private:
            /** The IPV4 address */
            sockaddr_in     address;

            // Address Interface
        public:
            /** Get the address as a UTF-8 textual value */
            virtual String asText() const;
            /** Set the address as a UTF-8 textual value
                @return false if the given value can not be parsed */
            virtual bool asText(const String & textualValue);

            /** Get the port if specified in the address (0 otherwise) */
            virtual uint16 getPort() const;
            /** Set the port if possible in this address
                @return false if the given port isn't available */
            virtual bool setPort(const uint16 port);

            /** Get the protocol (if specified in this address) */
            virtual Protocol getProtocol() const;
            /** Set the protocol (if specified in this address)
                @return false if the given protocol isn't supported */
            virtual bool setProtocol(const Protocol protocol);

            /** Check if the address is valid.
                @note A valid address is everything except 255.255.255.255
                If you want to discriminate also against 0.0.0.0, you might want to use isRoutable() */
            virtual bool isValid() const;
            /** Check if the address is routable.
                This includes everything except 0.0.0.0 and 255.255.255.255 */
            bool isRoutable() const { return isValid() && getSpecialAddressType() != Zero; }


            /** Get a bindable address.
                This method is used to ensure an address is bindable by a socket.
                @warning This method is useless for consumer code, it's used by the socket class if it doesn't understand the initial base address
                @return A base address that can be used to be bound correctly */
            virtual const BaseAddress & getBindableAddress() const;
            /** Get the low level object for this address
                @warning Don't use this, it's not what you think it is */
            virtual const void * getLowLevelObject() const;
            /** Get the low level object size for this address
                @warning Don't use this, it's not what you think it is */
            virtual const int getLowLevelObjectSize() const;

            /** Get type id for this address.
                This is used instead of RTTI */
            virtual int getTypeID() const { return 1; }
            /** Clone this address
                @return a pointer on a new allocated address you must delete. */
            virtual BaseAddress * clone() const { return new IPV4(*this); }

            // Our specific interface
        public:
            /** Query the name server from an textual address
                @param host     The host name to query
                @param timeout  The timeout in millisecond to wait for the answer
                @return An valid IPV4 address on success */
            static IPV4 queryNameServer(const String & host, const Time::TimeOut & timeout = DefaultTimeOut);
            /** Reverse query the name server for a textual address
                @param timeout  The timeout in millisecond to wait for the answer
                @return the host address that would match our name */
            String reverseQueryNameServer(const Time::TimeOut & timeout = DefaultTimeOut) const;

            /** Get the number of local available interfaces.
                @warning This might require privileged rights to get a valid result */
            static int  getLocalInterfacesCount();
            /** Get the local interfaces address
                @warning This might require privileged rights to get a valid result
                @param index    The interface index (0 based)
                @return The local interface address if found and set */
            static IPV4 getLocalInterfaceAddress(const int index);
            /** Get the local interface details.
                @warning This might require privileged rights to get a valid result
                @param index    The interface index (0 based)
                @param address  On output, contains the interface address
                @param mask     On output, contains the interface mask
                @param gateway  On output, contains the gateway address
                @param type     On output, contains the inferred interface type (Ethernet / Dialup / Wifi)
                @param macAddr  On output, contains the MAC address
                @param adapterName  If set, filled with the adapter name (eth0 on linux, GUID on windows)
                @return true on success, false otherwise */
            static bool getLocalInterfaceDetails(const int index, IPV4 & address, IPV4 & mask, IPV4 & gateway, Type & type, String & macAddr, String * adapterName = 0);
            /** Check if an address is routed on the given interface.
                @warning This might require privileged rights to get a valid result
                @param index The interface index (0 based)
                @return true if the address is routed on the interface */
            bool isUsingInterface(const int index) const;
            /** If the interface type is Wifi, this method will return the connected SSID (if any)
                and the key (WEP / WPA/PSK)
                @warning This method requires administrative privilege to be run.
                @warning Currently this method only runs on Windows
                @param index    The interface index (0 based)
                @param SSID     On output, contains the SSID this interface is connected to
                @param key      On output, contains the network key (if found and/or known), prefixed by "wep:" or "wpa:" or "wpa/psk:" and so on
                @return true on success, false otherwise */
            static bool getWIFIDetails(const int index, String & SSID, String & key);
            /** Get the MAC address of a remote device, using ARP protocol
                @param macAddress  The device's MAC address
                @return true on success, false if the IP address doesn't match a MAC address */
            bool getRemoteMACAddress(String & macAddress) const;
#if WantPingCode == 1
            /** Ping this address.
                This sends a ICMP probe on an host, and wait for a reply.
                Using this might requires special priviledges on some platform.
                @param timeout  The time to wait for an answer in milliseconds
                @return The round trip time in millisecond on success, negative on error,
                        negative round trip time if the answer is not from the expected address, -1 on any other error */
            int pingDevice(const Time::TimeOut & timeout = DefaultTimeOut) const;
#endif
            /** Get the class type for this address.
                @sa Class */
            Class getClassType() const;

            /** Get the special address type for this address.
                Use this method to figure out if an interface address is on a private network
                @sa SpecialAddress */
            SpecialAddress getSpecialAddressType() const;

            /** Check if a connection to the given address will require NAT passthrough system.
                @warning This only use special address type information.
                @warning No connection is actually performed to assert the returned result. */
            bool isNATPunchRequiredToConnect(const IPV4 & address) const;

            /** Get broadcast address for this network
                This returns the closest broadcast address for the current network (for example, 192.168.1.255) */
            IPV4 getBroadcastAddress() const;

            /** Get the next IP address on our subnet.
                @warning This method doesn't check for network mask, so the result is likely meaningless. */
            IPV4 & operator ++();
            /** Get the previous IP address on our subnet.
                @warning This method doesn't check for network mask, so the result is likely meaningless. */
            IPV4 & operator --();


            // Construction and destruction
        public:
            /** Construct an IPV4 address, in the form a.b.c.d:port
                The port is in this machine native order */
            IPV4(const uint8 a = 255, const uint8 b = 255, const uint8 c = 255, const uint8 d = 255, const uint16 port = 0);
            /** Construct an IPV4 address from a formal full 32 bits address in the form addr:port.
                The address and port are in this current machine native order */
            IPV4(const uint32 addr, const uint16 port = 0);
            /** Construct an IPV4 address from another address */
            IPV4(const IPV4 & other);
        };

        /** The IPV6 address is also a kind of network address.
            @warning this class isn't implemented fully yet */
        class IPV6 : public BaseAddress
        {
            // Members
        private:
            /** The IPv6 address */
            sockaddr_in6     address;

            // Address Interface
        public:
            /** Get the address as a UTF-8 textual value */
            virtual String asText() const;
            /** Set the address as a UTF-8 textual value
                @return false if the given value can not be parsed */
            virtual bool asText(const String & textualValue);

            /** Get the port if specified in the address (0 otherwise) */
            virtual uint16 getPort() const;
            /** Set the port if possible in this address
                @return false if the given port isn't available */
            virtual bool setPort(const uint16 port);

            /** Get the protocol (if specified in this address) */
            virtual Protocol getProtocol() const;
            /** Set the protocol (if specified in this address)
                @return false if the given protocol isn't supported */
            virtual bool setProtocol(const Protocol protocol);

            /** Check if the address is valid */
            virtual bool isValid() const;


            /** Get a bindable address.
                This method is used to ensure an address is bindable by a socket.
                @warning This method is useless for consumer code, it's used by the socket class if it doesn't understand the initial base address
                @return A base address that can be used to be bound correctly */
            virtual const BaseAddress & getBindableAddress() const;
            /** Get the low level object for this address
                @warning Don't use this, it's not what you think it is */
            virtual const void * getLowLevelObject() const;
            /** Get the low level object size for this address
                @warning Don't use this, it's not what you think it is */
            virtual const int getLowLevelObjectSize() const;

            /** Get type id for this address.
                This is used instead of RTTI */
            virtual int getTypeID() const { return 2; }
            /** Clone this address
                @return a pointer on a new allocated address you must delete. */
            virtual BaseAddress * clone() const { return new IPV6(*this); }

            // Our specific interface
        public:
            /** Query the name server from an textual address
                @param host     The host name to query
                @param timeout  The timeout in millisecond to wait for the answer
                @return An valid IPV6 address on success */
            static IPV6 queryNameServer(const String & host, const Time::TimeOut & timeout = DefaultTimeOut);

            // Construction and destruction
        public:
            /** Construct an IPv6 address, in the form
                a:b:c:d:e:f:g:h, port
                The port is in this machine native order */
            IPV6(const uint16 a = 0xffff, const uint16 b = 0xffff, const uint16 c = 0xffff, const uint16 d = 0xffff,
                 const uint16 e = 0xffff, const uint16 f = 0xffff, const uint16 g = 0xffff, const uint16 h = 0xffff,
                 const uint16 port = 0);
            /** Construct an IPv6 address, by giving the address and port in the host format */
            IPV6(const uint8 (&address)[16], const uint16 port);
            /** Construct an IPv6 address, by giving the address and port in the host format */
            explicit IPV6(const uint8 * address, const uint16 port);
            /** Construct an IPv6 address from another address */
            IPV6(const IPV6 & other);
        };

        /** The URL is a kind of network address following RFC3986.
            If you don't know what address to use, you should use this,
            as it's the larger standard.

            The convention for a URL is the following:
            @verbatim
            https://username:password@server/path/to/file.html?query=example#hook1

            /\__/\ /\_____________________/\/\______________/\/\__________/\/\___/\
            Scheme     Authority                 Path              Query    Fragment

            which can be represented as:
            scheme://authority/path?query#fragment
            @endverbatim */
        class URL : public BaseAddress
        {
            // Members
        protected:
            /** The scheme, also called protocol */
            String  scheme;
            /** The authority, usually called the server */
            String  authority;
            /** The path to the resource */
            String  path;
            /** The query */
            String  query;
            /** The fragment to reach */
            String  fragment;

            /** The IPv4 address matching the DNS'ed authority */
            mutable IPV4    address;
            /** The IPv6 address matching the DNS'ed authority */
            mutable IPV6    address6;

            // Helpers
        private:
            /** Split a text based URI */
            bool splitURI(const String & inputURL);
            /** Normalize a given path */
            void normalizePath(String & pathToNormalize) const;
            /** Normalize a given path */
            String normalizedPath(const String & pathToNormalize) const { String path(pathToNormalize); normalizePath(path); return path.normalizedPath('/', false); }


        public:
            /** Get the address as a UTF-8 textual value */
            virtual String asText() const;
            /** Set the address as a UTF-8 textual value
                @return false if the given value can not be parsed */
            virtual bool asText(const String & textualValue);

            /** Get the port if specified in the address (0 otherwise) */
            virtual uint16 getPort() const;
            /** Set the port if possible in this address
                @return false if the given port isn't available */
            virtual bool setPort(const uint16 port);

            /** Get the protocol (if specified in this address) */
            virtual Protocol getProtocol() const;
            /** Set the protocol (if specified in this address)
                @return false if the given protocol isn't supported */
            virtual bool setProtocol(const Protocol protocol);

            /** Check if the address is valid */
            virtual bool isValid() const;

            /** Get a bindable address.
                This method is used to ensure an address is bindable by a socket.
                @warning This method is useless for consumer code, it's used by the socket class if it doesn't understand the initial base address
                @return A base address that can be used to be bound correctly */
            virtual const BaseAddress & getBindableAddress() const;
            /** Get the low level object for this address
                @warning Don't use this, it's not what you think it is */
            virtual const void * getLowLevelObject() const;
            /** Get the low level object size for this address
                @warning Don't use this, it's not what you think it is */
            virtual const int getLowLevelObjectSize() const;

            /** Get type id for this address.
                This is used instead of RTTI */
            virtual int getTypeID() const { return 3; }
            /** Clone this address
                @return a pointer on a new allocated address you must delete. */
            virtual BaseAddress * clone() const { return new URL(*this); }

            // Interface
        public:
            /** Construct a text from this URL
                @param defaultScheme    The default scheme if missing */
            String asURI(const String & defaultScheme = "") const;
            /** Append path from the given path */
            URL appendRelativePath(String newPath) const;
            /** The escaping rules */
            enum EscapingRules
            {
                QueryOnly   =   0,   //!< Escape URI's query parameters (this includes escaping "=" ":" "/" and other)
                CompleteURI =   1,   //!< Escape the complete URI (this doesn't include "=" ":" and so on)
            };
            /** Escape an URL to only allowed chars
                @param inputURL   The URI to escape
                @param trimString If true, the URI is first trimmed from spaces and tab and whatever whitespace like characters
                @return the escaped URI */
            static String escapedURI(const String & inputURL, EscapingRules rules, const bool trimString = true);
            /** Unescape an URL to the initial string */
            static String unescapedURI(const String & inputURL);
            /** Append a variable to the query string (this perform URL text escaping on-the-way)
                @param name     The name of the variable to append
                @param value    The variable content
                @param forceAppending If not set and the variable with that name already exist, the function returns false and nothing is appended. Else, the new variable is appended anyway.
                @return true if the variable was appended correctly, false if it already exists (it's still appended in that case) */
            bool appendQueryVariable(String name, const String & value, const bool forceAppending = false);
            /** Find a variable value in the query.
                A URL query name can be like this:
                @verbatim
                    http://auth/path?a&b&c
                    In that case, findQueryVariable will return true for name = a, b or c, and the value will be empty (and false for name = d)
                    http://auth/path?a=3&b&c=4
                    In that case, findQueryVariable will return true for name = a, b or c, and the value will be respectively 3, empty, 4 (and false for name = d)
                    http://auth/path?a[]=3&a[]=4
                    In that case, findQueryVariable will return true for the name a (only), but only the first value 3 is returned.
                @endverbatim
                @param name     The variable name to look for
                @param value    On output, if found, it's set to the value of the variable unescaped
                @return true if found. */
            bool findQueryVariable(String name, String & value) const;
            /** Clear the query variable in the URL.
                @param name     The variable name to look for
                @return true if variable not present in query(even not find in query) */
            bool clearQueryVariable(String name);
            /** Clear the query */
            inline void clearQuery() { query = ""; }
            /** Strip port information from authority and return it if known */
            uint16 stripPortFromAuthority(uint16 defaultPortValue, const bool saveNewAuthority = true);
            /** Compare this URL with another URL ignoring some parts
                @param url             The URL to compare with
                @param cmpScheme       If false, ignore the scheme
                @param cmpAuthority    If false, ignore the authority
                @param cmpPath         If false, ignore the path
                @param cmpQuery        If false, ignore the query
                @param cmpFragment     If false, ignore the fragment
                @return true if both URL match (with the applied mask) */
            bool compareWith(const URL & url, const bool cmpScheme = true, const bool cmpAuthority = true, const bool cmpPath = true, const bool cmpQuery = true, const bool cmpFragment = false) const
            {
                return ((cmpScheme && scheme == url.scheme) || !cmpScheme) && ((cmpAuthority && authority == url.authority) || !cmpAuthority) && ((cmpPath && normalizedPath(path) == normalizedPath(url.path)) || !cmpPath) &&
                       ((cmpQuery && unescapedURI(query) == unescapedURI(url.query)) || !cmpQuery) && ((cmpFragment && fragment == url.fragment) || !cmpFragment);
            }


            // Accessor
        public:
            /** Get the current authority */
            inline const String & getAuthority() const  { return authority; }
            /** Get the current scheme */
            inline const String & getScheme() const     { return scheme; }
            /** Get the current path */
            inline const String & getPath() const       { return path; }
            /** Get the current query */
            inline const String & getQuery() const      { return query; }
            /** Get the current fragment */
            inline const String & getFragment() const   { return fragment; }

            /** Set the authority string */
            inline URL & setAuthority(const String & newAuth)   { authority = newAuth; return *this; }
            /** Set the scheme string */
            inline URL & setScheme(const String & newScheme)    { scheme = newScheme; return *this; }
            /** Set the current path */
            inline URL & setPath(const String & newPath)        { path = newPath; return *this; }
            /** Set the query string */
            inline URL & setQuery(const String & newQuery)      { query = newQuery; return *this; }
            /** Set the fragment string */
            inline URL & setFragment(const String & newFragment){ fragment = newFragment; return *this; }



        public:
            /** Default, and invalid constructor */
            URL();
            /** Construct an URL from a UTF8 text */
            URL(const String & inputURL, const String & defaultScheme = "");
            /** Construct an URL from its part */
            URL(const String & _scheme, const String & _authority, const String & _path, const String & _query = "", const String & _fragment = "");
        };

  #if _LINUX == 1
        /** This is for raw Ethernet sockets.
            This is defined in IEEE 802.3
            This is usually not accessible, unless you run Linux and have RAW socket creation privilege.
            @warning You must not use theses addresses for any other socket code than EthernetSocket, else it might crash or worse, it might kindawork */
        class Ethernet : public BaseAddress
        {
            // Type definition and enumeration
        public:
            /** The allowed ethernet protocols */
            enum EthProtocols
            {
                IPv4        = 0x0800, //!< Internet Protocol version 4 (IPv4)
                ARP         = 0x0806, //!< Address Resolution Protocol (ARP)
                WOL         = 0x0842, //!< Wake-on-LAN
                ReverseARP  = 0x8035, //!< Reverse Address Resolution Protocol
                AppleTalk   = 0x809B, //!< AppleTalk (Ethertalk)
                AARP        = 0x80F3, //!< AppleTalk Address Resolution Protocol (AARP)
                VLAN        = 0x8100, //!< VLAN-tagged frame (IEEE 802.1Q) and Shortest Path Bridging IEEE 802.1aq with NNI compatibility[10]
                IPX         = 0x8137, //!< IPX
                IPv6        = 0x86DD, //!< Internet Protocol Version 6 (IPv6)
            };

            // Members
        private:
            /** The address itself */
            uint8 address[6];
            /** The protocol for this address */
            uint16 protocol;

        public:
            /** Get the address as a UTF-8 textual value */
            virtual String asText() const;
            /** Set the address as a UTF-8 textual value
                @return false if the given value can not be parsed */
            virtual bool asText(const String & textualValue);

            /** Get the port if specified in the address (0 otherwise) */
            virtual uint16 getPort() const { return 0; }
            /** Set the port if possible in this address
                @return false if the given port isn't available */
            virtual bool setPort(const uint16 port) { return false; }

            /** Get the protocol (if specified in this address) */
            virtual Protocol getProtocol() const;
            /** Set the protocol (if specified in this address)
                @return false if the given protocol isn't supported */
            virtual bool setProtocol(const Protocol protocol);

            /** Check if the address is valid */
            virtual bool isValid() const;

            /** Get a complete address string (used for debugging mainly) including port and/or protocol if it makes sense */
            virtual String identify() const { return asText(); }

            /** Get a bindable address.
                This method is used to ensure an address is bindable by a socket.
                @warning This method is useless for consumer code, it's used by the socket class if it doesn't understand the initial base address
                @warning A reference on a static variable is returned so this code is not multithread safe.
                @return A base address that can be used to be bound correctly if the given Ethernet address is matching a local interface, else a null address (invalid) */
            virtual const BaseAddress & getBindableAddress() const;
            /** Get the low level object for this address
                @warning Don't use this, it's not what you think it is */
            virtual const void * getLowLevelObject() const { return address; }
            /** Get the low level object size for this address
                @warning Don't use this, it's not what you think it is */
            virtual const int getLowLevelObjectSize() const { return 0; }

            /** Get type id for this address.
                This is used instead of RTTI */
            virtual int getTypeID() const { return 4; }
            /** Clone this address
                @return a pointer on a new allocated address you must delete. */
            virtual BaseAddress * clone() const { return new Ethernet(address, protocol); }

            /** Required operator */
            Ethernet & operator = (const BaseAddress & other) { if (&other != this && other.getTypeID() == 4) { memcpy(address, static_cast<const Ethernet &>(other).address, ArrSz(address)); protocol = other.getProtocol(); } return *this; }

        public:
            /** Check if this address is multicast */
            bool isMulticast() const { return isValid() && (address[0] & 1) == 1; }
            /** Check if this address is a global address (OUI) */
            bool isGlobal() const { return isValid() && (address[0] & 2) == 0; }
            /** Get the OUI identifier */
            String getOUI() const { if (!isGlobal()) return ""; return String::Print("%02X%02X%02X", address[0], address[1], address[2]); }
            /** Get the adapter's name for the given address */
            String getAdapterName() const;
            /** Construct the address from the given adapter name */
            static Ethernet * fromAdapterName(const String & name);

            // Construction and Destruction
        public:
            /** Build an ethernet address from the MAC address */
            Ethernet(const uint8 (&_address)[6], const uint16 type = IPv4) : protocol(type) { memcpy(address, _address, ArrSz(address)); }
            /** Default constructor */
            Ethernet(const uint8 * _address = 0, const int len = 0, const uint16 type = IPv4) : protocol(type) { memset(address, 0, ArrSz(address)); if (_address && len == 6) memcpy(address, _address, len); }
        };
  #endif
        /** A NAT address hold 2 addresses, usually the private address (default), and the public address */
        class NAT : public IPV4
        {
            // Type definition and enumeration
        public:
            /** The NAT type */
            enum Type
            {
                Unknown     =   0,
                Cone        =   1,
                Symetric    =   2,
            };
            // Members
        private:
            /** The output (public) socket address */
            IPV4 *      publicAddress;
            /** The NAT type (if known) */
            Type        NATType;
            /** The NAT hop size */
            //int         hopSize;


        public:
            /** Get the public address (if behind NAT)
                @return 0 for local, unNAT'd address, or a pointer to the public address */
            virtual BaseAddress * getPublicAddress() { return publicAddress; }
            /** Set the public address (if behind NAT)
                @return false if unsupported */
            virtual bool setPublicAddress(BaseAddress * _publicAddress, const int type = 0) { delete publicAddress; if (_publicAddress->getTypeID() == 1) publicAddress = (IPV4*)_publicAddress; NATType = (Type)type; return true; }
            /** Get type id for this address.
                This is used instead of RTTI */
            virtual int getTypeID() const { return 5; }
            /** Clone this address
                @return a pointer on a new allocated address you must delete. */
            virtual BaseAddress * clone() const { return new NAT(*this); }

        };

        /** A typical IP address pair.
            This is a convenient method for storing the addresses used of both side of a Socket.
            This will typically be used in NAT based network topology where such pair should be changed once the NAT translating is known.
            @sa Network::Socket::BaseSocket */
        class AddressPair
        {
            // Members
        private:
            /** The local base address */
            BaseAddress * local;
            /** The remote address */
            BaseAddress * remote;

            // Helper methods
        private:
            /** Clear this object */
            void Clear() { delete0(this->local); delete0(this->remote); }

            // Interface
        public:
            /** Get the base address used for a socket */
            const BaseAddress & getLocalAddress() const { return *local; }
            /** Get the base address used for a socket */
            const BaseAddress & getRemoteAddress() const { return *remote; }
            /** Update address pair
                @param local    A pointer to a new allocated address that's owned by the class
                @param remote   A pointer to a new allocated address that's owned by the class */
            void updateAddressPair(BaseAddress * local, BaseAddress * remote) { Clear(); this->local = local; this->remote = remote; }
            /** Change the remote port.
                This is should work when using Datagram based socket, without binding the address, and using BaseSocket::sendTo().
                @warning Usually, changing a port after it has been bound and/or used is not a good idea, and will lead to subtle bugs that are hard to diagnose
                @param remotePort   The remote port to use from now (in host format, as usual) */
            bool setRemotePort(const uint16 port) { return remote->setPort(port); }


            /** Construct an address pair from a local and remote address
                @param local    A pointer to a new allocated address that's owned by the class
                @param remote   A pointer to a new allocated address that's owned by the class */
            AddressPair(BaseAddress * local = 0, BaseAddress * remote = 0) : local(local), remote(remote) {}
            ~AddressPair() { Clear(); }
        };
    }
}

#endif
