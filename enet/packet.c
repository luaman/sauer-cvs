#include <string.h>
#include <enet/memory.h>
#include <enet/enet.h>

ENetPacket *
enet_packet_create (const void * data, size_t dataLength, uint32 flags)
{
    ENetPacket * packet = (ENetPacket *) enet_malloc (sizeof (ENetPacket));

    packet -> data = (uint8 *) enet_malloc (dataLength);

    if (data != NULL)
      memcpy (packet -> data, data, dataLength);

    packet -> referenceCount = 0;
    packet -> flags = flags;
    packet -> dataLength = dataLength;

    return packet;
}

void
enet_packet_destroy (ENetPacket * packet)
{
    enet_free (packet -> data);
    enet_free (packet);
}

int
enet_packet_resize (ENetPacket * packet, size_t dataLength)
{
    uint8 * newData;
   
    if (dataLength <= packet -> dataLength)
    {
       packet -> dataLength = dataLength;

       return 0;
    }

    newData = (uint8 *) enet_realloc (packet -> data, dataLength);

    packet -> data = newData;
    packet -> dataLength = dataLength;

    return 0;
}

