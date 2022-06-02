#ifndef hpp_MQTTConfig_hpp
#define hpp_MQTTConfig_hpp

/** Is the protocol going to be used as a client only or as a broker.
    Default: 1. */
#define MQTTClientOnlyImplementation 1

/** Authentication support. Set to 1 if your broker is using and expecting AUTH packet for connection.
    Typically unused for the majority of broker, this saves binary size if left disabled
    If enabled, you have to implement the authReceived callback to feed the server with valid authentication data.
    Default: 0 */
#define MQTTUseAuth 0

/** Unsubscribe support. Set to 1 if you intend to unsubscribe dynamically and partially from the broker.
    Typically unused for the majority of embedded case where the client is subscribing all topics at once and let 
    the broker unsubscribe by itself upon disconnection, this saves binary size if left disabled
    Default: 0 */
#define MQTTUseUnsubscribe 0


/** Dump all MQTT communication. 
    This causes a large increase in binary size, induce an important latency cost, and lower the security by 
    displaying potentially private informations 
    Default: 0 */ 
#define MQTTDumpCommunication 0

/** Remove all validation from MQTT types.
    This removes validation check for all MQTT types in order to save binary size. 
    This is only recommanded if you are sure about your broker implementation (don't set this to 1 if you 
    intend to connect to unknown broker)
    Default: 0 */
#define MQTTAvoidValidation 1

/** Enable SSL/TLS code.
    If your broker is on the public internet, it's a good idea to enable TLS to avoid communication snooping.
    This adds a large impact to the binary size since the socket code is then duplicated (SSL and non SSL).
    The SSL socket code provided is using mbedtls, but one could use BearSSL if size is really limited instead.

    Please notice that this has no effect if MQTTOnlyBSDSocket is 0, since ClassPath embeds its own SSL socket 
    code (abstracted away at a higher level)
    Default: 1 */
#ifndef MQTTUseTLS
  #define MQTTUseTLS          0
#endif

/** Simple socket code.
    If set to true, this disables the optimized network code from ClassPath and fallback to the minimal subset 
    of BSD socket API (typically send / recv / connect / select / close / setsockopt).
    This also limits binary code size but prevent using SSL/TLS (unless you write a wrapper for it).
    This is usually enabled for embedded system with very limited resources.
    
    Please notice that this also change the meaning of timeout values. If it's not set, then timeouts represent 
    the maximum time that a method could spend (including all sub-functions calls). It's deterministic. 
    When it's set, then timeouts represent the maximum inactivity time before any method times out. So if you 
    have a very slow connection sending 1 byte per the timeout delay, in the former case, it'll timeout after 
    the first byte is received, while in the latter case, it might never timeout and take up to 
    `timeout * packetLength` time to return.

    
    Default: 0 */
#ifndef MQTTOnlyBSDSocket 
  #define MQTTOnlyBSDSocket   1
#endif

// The part below is for building only, it's made to generate a message so the configuration is visible at build time
#if _DEBUG == 1
  #if MQTTUseAuth == 1
    #define CONF_AUTH "Auth_"
  #else
    #define CONF_AUTH "_"
  #endif

  #if MQTTUseUnsubscribe == 1
    #define CONF_UNSUB "Unsub_"
  #else
    #define CONF_UNSUB "_"
  #endif

  
  #if MQTTDumpCommunication == 1
    #define CONF_DUMP "Dump_"
  #else
    #define CONF_DUMP "_"
  #endif
  
  #if MQTTAvoidValidation == 1
    #define CONF_VALID "Check_"
  #else
    #define CONF_VALID "_"
  #endif
  
  #if MQTTUseTLS == 1
    #define CONF_TLS "TLS_"
  #else
    #define CONF_TLS "_"
  #endif
  
  #if MQTTOnlyBSDSocket == 1
    #define CONF_SOCKET "BSD"
  #else
    #define CONF_SOCKET "CP"
  #endif
  
  #pragma message("Building eMQTT5 with flags: " CONF_AUTH CONF_UNSUB CONF_DUMP CONF_VALID CONF_TLS CONF_SOCKET)
#endif

#endif
