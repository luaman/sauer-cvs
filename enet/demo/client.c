#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <enet/enet.h>

int
main (int argc, char ** argv)
{
   ENetAddress address = { ENET_HOST_ANY, 6666 };
   ENetHost * host = enet_host_create (NULL, atoi (argv [2]), 64 * 1024 * 1024, 64 * 1024 * 1024);
   uint32 lastSend = enet_time_get ();
   ENetEvent event;
   int connections;

   if (argc < 4)
     return EXIT_FAILURE;

   if (host == NULL)
     return EXIT_FAILURE;

   enet_address_set_host (& address, argv [1]);

   connections = atoi (argv [2]);

   while (connections -- > 0)
     enet_host_connect (host, & address, 1);

   for (;;)
   {
      if (enet_host_service (host, & event, 0) < 0)
        break;
   
      if (enet_time_get () - lastSend >= 33)
      {
         enet_host_broadcast (host,
                              0,
                              enet_packet_create (argv [3],
                                                  strlen (argv [3]) + 1,
                                                  (rand () % 30) == 0 ? ENET_PACKET_FLAG_RELIABLE : 0));

         lastSend = enet_time_get ();
      }

      switch (event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
         printf ("Connected!\n");

         break;
         
      case ENET_EVENT_TYPE_RECEIVE:
         printf ("Received %d bytes in %u ms from peer %u with throttle at %f and limit %u\n", 
                 event.packet -> dataLength, event.peer -> roundTripTime, event.peer -> incomingPeerID,
                 event.peer -> packetThrottleLimit / (float) ENET_PEER_PACKET_THROTTLE_SCALE,
                 event.peer -> incomingBandwidth);
         
         enet_packet_destroy (event.packet);

         break;

      case ENET_EVENT_TYPE_DISCONNECT:
         printf ("Disconnected!\n");

         enet_host_destroy (host);

         return EXIT_SUCCESS;
      }
   }

   enet_host_destroy (host);

   return EXIT_FAILURE;
}

