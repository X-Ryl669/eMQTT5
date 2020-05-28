# API documentation

## Low level API
This MQTT client is using Object Oriented Programming to map the MQTT v5.0 specification into code.
All communication elements in [MQTT](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html) are described in control packet and are represented in this library as a `ControlPacketSerializable`

This class hierarchy implements the `Serializable` interface used to map to/from in memory representation from/to an object usable from C/C++.

Because all control packets are different and have their own quirks, this library use a template-based structures declaration that are specialized by the `ControlPacketType`.

In order to limit the generated binary size that would happen otherwise with the compiler creating as many instance as there are different control packets, the common behavior is implemented in 
base classes in virtual methods and only the specificity for each packet are overloaded.

Except for `SubscribeTopic` and `Properties`, all other classes do not allocate on the heap.
So using this library is possible with only stack based allocation (useful to avoid heap fragmentation or limited heap memory available).
`SubscribeTopic` (resp. `UnsubscribeTopic`) can be constructed to use heap based allocation for their chained list for each topic in a packet (default behavior) or can be constructed on a stack.
In that case, while being destructed, only heap stored object will be deleted.  

Please notice that when receiving MQTT packet, this client never receive a Subscribe/Unsubscribe packet (only a broker does). Thus, there is no allocation made in that case.

Concerning properties, the client only generates views on the incoming data and does not allocate anything via the `PropertiesView` class.


When generating properties for sending them (typically, in `CONNECT`, `PUBLISH` and `AUTH` packets), a mechanism of Copy-On-Write is used to avoid allocating useless properties.
By default `Properties` is a chained list with 2 heads (`head` and `reference`) that are pointing to the same node. As soon as a modification is made on the chained list, the `head` is modified (the `reference` is never modified).
When destructing, only the nodes between the `head` and the `reference` are destructed.
Each node in a `Properties` list implements the `PropertyBase` interface, and will be asked to `suicide()` when required to destruct.


Most `PropertyBase` instance are allocating on the stack by default, so using `Properties` and appending stack based `Property` node will result in a stack based chained list. It's perfectly fine to append a heap-allocated 
`Property` (but make sure you're telling it's heap allocated in its constructor), it'll be deleted correctly when `Properties` instance is destructed.

Please beware about the difference between `View` and `non-View` classes. The former only store a pointer on unmanaged memory (so the pointed memory must exist **as long as the instance of the view lives**) while the latter 
allocates and copy the data (so the pointed memory can be deallocate whenever you want after the constructor has finished).
In general, using `View` and being careful about destruction order is a major win for both limiting memory allocation size and improving runtime speed. 


The library can be used for writing a client or a broker. When writing a client, you should define the MQTTClientOnlyImplementation that will disable all code that's not required for a client.
 
