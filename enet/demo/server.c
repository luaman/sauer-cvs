#include <stdlib.h>
#include <stdio.h>
#include <enet/enet.h>

int
main (int argc, char ** argv)
{
   ENetAddress address = { ENET_HOST_ANY, 6666 };
   ENetHost * host = enet_host_create (& address, 64, 56 * 1024, 56 * 1024);
   ENetEvent event;

   if (host == NULL)
     return EXIT_FAILURE;

   for (;;)
   {
      if (enet_host_service (host, & event, 60000) <= 0)
        continue;
      
      switch (event.type)
      {
      case ENET_EVENT_TYPE_RECEIVE:
         enet_host_broadcast (host, 0, event.packet);

         break;
      }
   }

   enet_host_destroy (host);

   return EXIT_FAILURE;
}

