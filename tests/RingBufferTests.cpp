#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef unsigned int uint32;
typedef unsigned short int uint16;
typedef unsigned char uint8;

template <typename T>
static inline T min(T a, T b) { return a < b ? a : b; }


struct PacketBookmark
{
    uint16 ID;
    uint32 size;
    uint32 pos;

    inline void set(uint16 ID, uint32 size, uint32 pos) { this->ID = ID; this->size = size; this->pos = pos; }
    PacketBookmark(uint16 ID = 0, uint32 size = 0, uint32 pos = 0) : ID(ID), size(size), pos(pos) {}
};
struct Impl
{
    uint32           r, w;
    const uint32     sm1; // Size minus 1
    uint8 *          buffer;
    PacketBookmark * packets;
    uint8            packetsCount;        // Number of packets in the buffer

    // Used to map indexes circularly
    inline uint32 realIndex(uint32 iter) const
    {
        return (r + iter) & sm1;
    }

    uint8 findID(uint32 ID)
    {
        for (uint8 i = 0; i < packetsCount; i++)
            if (packets[i].ID == ID)
                return i;
        return packetsCount;
    }
    bool clearSetID(uint32 set, uint32 clear = 0)
    {
        uint8 i = findID(clear);
        if (i == packetsCount) return false;
        packets[i].ID = set;
        return true;
    }

    /** Get the consumed size in the buffer */
    inline uint32 getSize() const { return w >= r ? w - r : (sm1 - r + w + 1); }
    /** Get the available size in the buffer */
    inline uint32 freeSize() const { return sm1 - getSize(); }

    /** Add a packet to this buffer (no allocation is done at this time) */
    bool save(const uint16 packetID, const uint8 * packet, uint32 size)
    {
        // Check we can fit the packet
        if (size > sm1 || freeSize() < size) return false;
        // Check if we have a free space for storing the packet's information
        uint8 i = findID(0);
        if (i == packetsCount) return false;

        const uint32 part1 = min(size, sm1 - w + 1);
        const uint32 part2 = size - part1;

        memcpy((buffer + w), packet, part1);
        memcpy((buffer), packet + part1, part2);

        packets[i].set(packetID, size, w);
        w = (w + size) & sm1;
        return true;
    }
    /** Get a packet from the buffer.
        Since this is used to resend packet over a TCP (thus streaming) socket,
        we can skip one copy to rebuilt a contiguous packet by simply returning
        the two part in the ring buffer and let the application send them successively.
        From the receiving side, they'll be received as one contiguous packet.
     */
    bool load(const uint16 packetID, const uint8 *& packetHead, uint32 & sizeHead, const uint8 *& packetTail,  uint32 & sizeTail)
    {
        // Look for the packet
        uint8 i = findID(packetID);
        if (i == packetsCount) return false;

        // Check if the packet is split
        packetHead = buffer + packets[i].pos;
        sizeHead = min(packets[i].size, (sm1 - packets[i].pos + 1));
        sizeTail = packets[i].size - sizeHead;
        packetTail = buffer;
        return true;
    }

    /** Remove a packet from the buffer */
    bool release(const uint16 packetID)
    {
        uint8 i = findID(packetID);
        if (i == packetsCount) return false;

        PacketBookmark & packet = packets[i];

        // Here, we have 2 cases. Either the packet is on the read position of the ring buffer
        // and in that case, we just need to advance the read position.
        // Either it's in the middle of the ring buffer and we need to move all the data around to remove it
        // Let's deal with the former case first
        uint32 pos = packet.pos, size = packet.size, end = (packet.pos + packet.size) & sm1;
        packet.set(0, 0, 0);
        if (pos == r)
        {
            r = (r + size) & sm1;
            return true;
        }

        // Another optimization step is when the write position is at the end of this packet
        // We can just revert the storage of the packet directly
        if (end == w)
        {
            w = pos;
            return true;
        }

        // Ok, now we have to move the data around here
        // First let's move memory to remove that packet.
        // We'll fix the packet's position later on
        // We are in this case here:
        //  bbbbccw  r p   eaaaaaaaaa      with p/e (pos/end) and a / b / c the next packet
        //        |  | |   |
        // [-------------------------]
        // After move, it should look like:
        //  ccw       r aaaaaaaaabbbb      with p/e (pos/end) and a / b / c the next packet
        //    |       | |   |
        // [-------------------------]
        // We see they are 3 sections: a is the data between e and the buffer end
        // b is the data whose size is equal to buffer end - p - sizeof(a) (the part that was moved from
        // the beginning of the buffer to the end of the buffer)
        // and c is the part that was move from the end of the packet to the beginning of the buffer
        // We have to perform move a to p, move b to buffer.end - a.size and move c to buffer begin

        // It's hard to think without unwrapping the buffer, so let's imagine we are doing so (we'll rewrap after discussion)
        // Let's set w' = w+sm1+1, e' = e + (sm1 - 1)
        // It'll lead to this diagram:
        //  bbbbccw  r p   eaaaaaaaaa BBBBCCW       with p/e (pos/end) and a / b / c the next packet
        //        |  | |   |                |
        // [-------------------------:-------------------------]
        // Or
        //     eaacccccw  r        p     EAACCCCCW
        //     |       |  |        |     |       |
        // [-------------------------:-------------------------]
        // Or
        //      r  p    eaaaaa w
        //      |  |    |    | |
        // [-------------------------:-------------------------]
        // In that case, we are doing a single memory move operation here, but we simply wrap the position
        uint32 s = sm1 + 1, W = w < pos ? w + s : w, E = end < pos ? end + s : end;

        for (uint32 u = 0; u < W - E; u++)
            buffer[(u + pos) & sm1] = buffer[(u + pos + size) & sm1];

        // Adjust the new write position
        w = (W - size) & sm1;

        // We can split the packets in 2 cases: before or after the packet to remove.
        // We'll iterate each packet and decide if we need to move it (it's after the packet to remove)
        // This isn't the most efficient algorithm, but since the number of packets to store is small
        // there's no point in optimizing it further

        bool continueSearching = true;
        while (continueSearching)
        {
            continueSearching = false;
            for (uint8 j = 0; j < packetsCount; j++)
            {
                PacketBookmark & iter = packets[j];
                if (iter.pos == end)
                {
                    end = (iter.pos + iter.size) & sm1;
                    iter.pos = pos;
                    pos = (iter.pos + iter.size) & sm1;
                    continueSearching = true;
                    break;
                }
            }
        }
        return true;
    }

    bool selfCheck() const
    {
        for (uint8 j = 0; j < packetsCount; j++)
        {
            const PacketBookmark & i = packets[j];
            if (!i.ID) continue;
            const uint32 end = (i.pos + i.size) & sm1;
            if (end == w) continue;
            // Find if any packet starts with the next slot
            bool found = false;
            for (uint8 k = 0; k < packetsCount; k++)
            {
                if (packets[k].pos == end)
                {
                    // Found one
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                fprintf(stderr, "Error while checking packet %u (%u,s:%u), no next packet found and not tail position\n", i.ID, i.pos, i.size);
                return false;
            }
        }
        fprintf(stdout, "RB: r(%u) w(%u)\n", r, w);
        return true;
    }

    Impl(size_t size, uint8 * buffer, uint8 packetsCount, PacketBookmark * packets) : r(0), w(0), sm1(size - 1), buffer(buffer), packets(packets), packetsCount(packetsCount) {}
};

static unsigned long next = 1;

/* RAND_MAX assumed to be 32767 */
#define MYRAND_MAX  32767
int myrand(void) {
   next = next * 1103515245 + 12345;
   return((unsigned)(next/65536) % 32768);
}

void mysrand(unsigned int seed) {
   next = seed;
}

int main(int argc, char ** argv)
{
    const uint32 bufSize = 2048 * 4;
    const uint8 packetsCount = 4;

    // Create the instance
    Impl impl(bufSize, new uint8[bufSize], packetsCount, new PacketBookmark[packetsCount]);

    // Now feed the random dance here
    uint16 packetID = 1;

    volatile int seed = 0;
    if (argc > 1) seed = atoi(argv[1]);
    else
    {
        seed = time(NULL) ^ 0x3457FDEa;
    }
    mysrand(seed);

    fprintf(stdout, "Starting with seed: %d\n", seed);

    uint8 buffer[2048] = { 0xDE, 0xAD, 0xFA, 0xCE };
    // Store some packets in the implementation
    uint8 packetCount = 0; //(myrand() * packetsCount) / MYRAND_MAX;
    uint8 cleaningOrder = (myrand() * packetsCount - 1) / MYRAND_MAX;
    for (uint32 i = 0; i < 200; i++)
    {
        // Store packets in the buffer
        uint32 size = (myrand() * 2048) / MYRAND_MAX;
        if (size < 8) continue;
        // Fill the buffer with recognizable pattern
        buffer[size - 4] = 0xB1; buffer[size - 3] = 0x6B; buffer[size - 2] = 0x00; buffer[size - 1] = 0x0B;
        memset(buffer+4, packetID, size - 8);
        if (!impl.save(packetID++, buffer, size))
           return fprintf(stderr, "Error saving packet %u with size %u\n", packetID - 1, size);
        fprintf(stderr, "Saved packet %u with size %u\n", packetID - 1, size);
        packetCount++;

        impl.selfCheck();

        if (packetCount == packetsCount)
        {
            // Need to load up some packet to ensure it works
            const uint8 * head = 0, * tail = 0;
            uint32 h = 0, t = 0;
            const uint16 ID = packetID - packetCount + cleaningOrder;
            if (!impl.load(ID, head, h, tail, t))
               return fprintf(stderr, "Error loading packet %u with size %u\n", ID, h + t);

            // Check the buffer correctness now
            memcpy(buffer, head, h);
            memcpy(buffer + h, tail, t);
            if (buffer[0] != 0xDE || buffer[1] != 0xAD || buffer[2] != 0xFA || buffer[3] != 0xCE)
               return fprintf(stderr, "Invalid packet header %u with size %u\n", ID, h+t);

            uint32 s = h+t;
            if (buffer[s-4] != 0xB1 || buffer[s-3] != 0x6B || buffer[s-2] != 0x00 || buffer[s-1] != 0x0B)
               return fprintf(stderr, "Invalid packet tail %u with size %u\n", ID, s);

            // Then release the packet
            if (!impl.release(ID))
               return fprintf(stderr, "Can't release packet %u with size %u\n", ID, s);

            fprintf(stderr, "Released packet %u\n", ID);
            impl.selfCheck();
            packetCount--;
        }
    }
    delete[] impl.buffer;
    delete[] impl.packets;
    fprintf(stdout, "Done\n");
    return 0;
}
