# Using the client 

You need to include the file [MQTTClient.hpp](https://github.com/X-Ryl669/eMQTT5/blob/master/lib/include/Network/Clients/MQTT.hpp).
On top of this file, you need to specify your options by changing the various macro value (see documentation on each macro).

Typically, you'll find these macros:

1. **MQTTClientOnlyImplementation**: You are unlikely to change this macro
2. **MQTTUseAuth**: Whether in your protocol you are using the AUTH control packet
3. **MQTTDumpCommunication**: Useful for debugging, this dumps each packet sent and received, you'll need to turn this off for production
4. **MQTTAvoidValidation**: If enabled, all validation code is removed. You should only use this if you master the broker used in your installation and know it'll not send malformed packet
5. **MQTTOnlyBSDSocket**: Usually set to 1 for using plain old sockets. If set to 0, then more efficient, but larger ClassPath's network code is used
6. **MQTTUseTLS**: If enabled, you can connect to TLS based MQTT brokers. This add some overhead in binary code size (typically 5% more) and requires MbedTLS  

The client is located in `Network::Client::MQTTv5` class.
The main methods are:

1. **connectTo**: Establish a connection to the given MQTT server 
2. **auth**: Authenticate with the MQTT server
2. **subscribe** (2): Subscribe to one or more topic on the MQTT server 
3. **publish**: Publish a packet on a MQTT server 
4. **disconnect**: Disconnect from a MQTT server cleanly 
5. **eventLoop**: The method that needs to be called regularly (from a thread ?) for processing messages

Upon construction, a buffer for receiving packets (with a limited and specifiable size) is allocated on the heap.

In case your platform does not support heap allocation, this can easily be changed to a BSS/static based allocation in `Network::Client::MQTTv5::Impl` constructor. 

# Specificities of MQTT v5.0
MQTT v5.0 introduced new features compared to MQTT v3.1.1: mainly Properties and Authentication

## Authentication
Authentication in MQTT v3.1.1 was mainly either client identifier based or username/password based, a bit like HTTP/1.0 did.
In MQTT v5.0, it's now possible to also have multiple step authentication with per-broker/per-client specific protocol.
Authentication requires a new control packet type, and, as such, you can use **auth** method of the client to build this packet.

The usual process with authentication is the following:

1. Client => `CONNECT with some authentication properties attached` => Server
2. Server => `AUTH with some challenge/method/data` => Client
3. Client => `AUTH with some answer/data`=> Server
4. Server => `CONNACK` => Client

For the first step, you'll need to append as many properties as required (see the Properties section below for how to do that) in your first call to **connectTo**.

When the server answers (in step 2), your callback instance will be called with the authentication method and data already parsed for you.

You'll then use the **auth** method in step 3 to complete the challenge, the method either returns with a success (step 4) or a failure.

Please notice that none of the above is required for usual client identifier or username / password connection.

## Using Properties with the client

Since this library is oriented for embedded usage, a great care was taken for avoiding heap usage and minimizing code size.

### Packet in destination of the broker
In order to acheive theses goals, the `Properties` class is a chained list where each node stores a flag telling if it was allocated on the stack (default) or on the heap.

When the chained list is destructed, each node will either suicide(delete itself) or just chain the destruction request to the next node.

Parsing the chained list is done recursively, but this shouldn't be an issue since the number of possible properties for each packet is small.

Appending properties is done like this:

```
    Property<uint32> maxProp(PacketSizeMax, recvBufferSize);
    
    if (!packet.props.getProperty(PacketSizeMax))
        packet.props.append(&maxProp); // That's possible with a stack property as long as the lifetime of the object outlive the packet
```

### Receiving packets from the broker
When receiving a packet, the code never does any copy, so serialization from Properties is done directly from the received buffer.
In that case, you'll be dealing with `PropertiesView` class and more specifically with its `bool getProperty(VisitorVariant & visitor, PropertyType & type, uint32 & offset) const` method.

Typically, you'll create a `VisitorVariant` instance, a `PropertyType` instance and an `offset` counter then call `getProperty`.
This method will fill each instance with the appropriate visitor and type. `offset` is increased to the next property position in the observed (received) buffer.
It's then up to you to check which property you are interested in, and extract the visited property value like this:

```
    PropertyType type = BadProperty;
    uint32 offset = 0;
    VisitorVariant visitor;
    while (packet.props.getProperty(visitor, type, offset))
    {
        switch (type)
        {
            case PacketSizeMax:
            {
                auto pod = visitor.as< LittleEndianPODVisitor<uint32> >();
                maxPacketSize = pod->getValue();
                break;
            }
   [...]
```


