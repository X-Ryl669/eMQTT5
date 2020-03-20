# API documentation

## Low level API
This MQTT client is using Object Oriented Programming to map the MQTT v5.0 specification into code.
All communication elements in [MQTT](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html) are described in control packet and are represented in this library as a `ControlPacketSerializable`

This class hierarchy implements the `Serializable` interface used to map to/from in memory representation from/to an object usable from C/C++.

Because all control packets are different and have their own quirks, this library use a template-based structures declaration that are specialized by the `ControlPacketType`.

In order to limit the generated binary size that would happen otherwise with the compiler creating as many instance as there are different control packets, the common behavior is implemented in 
base class in virtual methods and only the specificity for each packet are overloaded.

Except for `SubscribeTopic` and `Properties`, all other classes do not allocate on the heap and using this library is possible with only stack based allocation (useful to avoid heap fragmentation or limited heap memory available).
`SubscribeTopic` (resp. `Properties`) contains a chained list for each topic (resp. property) in a packet. A `StackSubscribeTopic` (resp. `StackProperty`) object can be used and linked in the list (the library knows about this and will not delete the instance).

The library can be used for writing a client or a broker. When writing a client, you should define the MQTTClientOnlyImplementation that will disable all code that's not required for a client.
 
