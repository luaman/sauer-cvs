#include <enet/memory.h>
#include <enet/enet.h>

ENetHost *
enet_host_create (const ENetAddress * address, size_t peerCount, uint32 incomingBandwidth, uint32 outgoingBandwidth)
{
    ENetHost * host = (ENetHost *) enet_malloc (sizeof (ENetHost));
    ENetPeer * currentPeer;

    host -> peers = (ENetPeer *) enet_calloc (peerCount, sizeof (ENetPeer));

    host -> socket = enet_socket_create (ENET_SOCKET_TYPE_DATAGRAM, address);
    if (host -> socket == ENET_SOCKET_NULL)
    {
       enet_free (host -> peers);
       enet_free (host);

       return NULL;
    }

    if (address != NULL)
      host -> address = * address;

    host -> incomingBandwidth = incomingBandwidth;
    host -> outgoingBandwidth = outgoingBandwidth;
    host -> bandwidthThrottleEpoch = 0;
    host -> recalculateBandwidthLimits = 0;
    host -> peerCount = peerCount;
    host -> lastServicedPeer = host -> peers;
    host -> commandCount = 0;
    host -> bufferCount = 0;
    host -> receivedAddress.host = ENET_HOST_ANY;
    host -> receivedAddress.port = 0;
    host -> receivedDataLength = 0;
     
    for (currentPeer = host -> peers;
         currentPeer < & host -> peers [host -> peerCount];
         ++ currentPeer)
    {
       currentPeer -> host = host;
       currentPeer -> incomingPeerID = currentPeer - host -> peers;
       currentPeer -> data = NULL;

       enet_list_clear (& currentPeer -> acknowledgements);
       enet_list_clear (& currentPeer -> sentReliableCommands);
       enet_list_clear (& currentPeer -> sentUnreliableCommands);
       enet_list_clear (& currentPeer -> outgoingReliableCommands);
       enet_list_clear (& currentPeer -> outgoingUnreliableCommands);

       enet_peer_reset (currentPeer);
    }
 
    return host;
}

void
enet_host_destroy (ENetHost * host)
{
    ENetPeer * currentPeer;

    enet_socket_destroy (host -> socket);

    for (currentPeer = host -> peers;
         currentPeer < & host -> peers [host -> peerCount];
         ++ currentPeer)
    {
       enet_peer_reset (currentPeer);
    }

    enet_free (host -> peers);
    enet_free (host);
}

ENetPeer *
enet_host_connect (ENetHost * host, const ENetAddress * address, size_t channelCount)
{
    ENetPeer * currentPeer;
    ENetChannel * channel;
    ENetProtocol command;

    if (channelCount < ENET_PROTOCOL_MINIMUM_CHANNEL_COUNT)
      channelCount = ENET_PROTOCOL_MINIMUM_CHANNEL_COUNT;
    else
    if (channelCount > ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT)
      channelCount = ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT;

    for (currentPeer = host -> peers;
         currentPeer < & host -> peers [host -> peerCount];
         ++ currentPeer)
    {
       if (currentPeer -> state == ENET_PEER_STATE_DISCONNECTED)
         break;
    }

    if (currentPeer >= & host -> peers [host -> peerCount])
      return NULL;

    currentPeer -> state = ENET_PEER_STATE_CONNECTING;
    currentPeer -> address = * address;
    currentPeer -> channels = (ENetChannel *) enet_malloc (channelCount * sizeof (ENetChannel));
    currentPeer -> channelCount = channelCount;

    for (channel = currentPeer -> channels;
         channel < & currentPeer -> channels [channelCount];
         ++ channel)
    {
        channel -> outgoingReliableSequenceNumber = 0;
        channel -> outgoingUnreliableSequenceNumber = 0;
        channel -> incomingReliableSequenceNumber = 0;
        channel -> incomingUnreliableSequenceNumber = 0;

        enet_list_clear (& channel -> incomingReliableCommands);
        enet_list_clear (& channel -> incomingUnreliableCommands);
    }
        
    command.header.command = ENET_PROTOCOL_COMMAND_CONNECT;
    command.header.channelID = 0xFF;
    command.header.flags = ENET_PROTOCOL_FLAG_ACKNOWLEDGE;
    command.header.commandLength = sizeof (ENetProtocolConnect);
    command.connect.outgoingPeerID = ENET_HOST_TO_NET_16 (currentPeer -> incomingPeerID);
    command.connect.packetSize = ENET_HOST_TO_NET_16 (currentPeer -> packetSize);
    command.connect.windowSize = ENET_HOST_TO_NET_32 (currentPeer -> windowSize);
    command.connect.channelCount = ENET_HOST_TO_NET_32 (channelCount);
    command.connect.incomingBandwidth = ENET_HOST_TO_NET_32 (host -> incomingBandwidth);
    command.connect.outgoingBandwidth = ENET_HOST_TO_NET_32 (host -> outgoingBandwidth);
    command.connect.packetThrottleInterval = ENET_HOST_TO_NET_32 (currentPeer -> packetThrottleInterval);
    command.connect.packetThrottleAcceleration = ENET_HOST_TO_NET_32 (currentPeer -> packetThrottleAcceleration);
    command.connect.packetThrottleDeceleration = ENET_HOST_TO_NET_32 (currentPeer -> packetThrottleDeceleration);
    
    enet_peer_queue_outgoing_command (currentPeer, & command, NULL, 0, 0);

    return currentPeer;
}

void
enet_host_broadcast (ENetHost * host, uint8 channelID, ENetPacket * packet)
{
    ENetPeer * currentPeer;

    for (currentPeer = host -> peers;
         currentPeer < & host -> peers [host -> peerCount];
         ++ currentPeer)
    {
       if (currentPeer -> state != ENET_PEER_STATE_CONNECTED)
         continue;

       enet_peer_send (currentPeer, channelID, packet);
    }

    if (packet -> referenceCount == 0)
      enet_packet_destroy (packet);
}

void
enet_host_bandwidth_limit (ENetHost * host, uint32 incomingBandwidth, uint32 outgoingBandwidth)
{
    host -> incomingBandwidth = incomingBandwidth;
    host -> outgoingBandwidth = outgoingBandwidth;
    host -> recalculateBandwidthLimits = 1;
}

void
enet_host_bandwidth_throttle (ENetHost * host)
{
    uint32 timeCurrent = enet_time_get (),
           elapsedTime = timeCurrent - host -> bandwidthThrottleEpoch,
           peersTotal = 0,
           dataTotal = 0,
           peersRemaining,
           bandwidth,
           throttle = 0,
           bandwidthLimit = 0;
    int needsAdjustment;
    ENetPeer * peer;
    ENetProtocol command;

    if (elapsedTime < ENET_HOST_BANDWIDTH_THROTTLE_INTERVAL)
      return;

    for (peer = host -> peers;
         peer < & host -> peers [host -> peerCount];
         ++ peer)
    {
        if (peer -> state != ENET_PEER_STATE_CONNECTED)
          continue;

        ++ peersTotal;
        dataTotal += peer -> outgoingDataTotal;
    }

    if (peersTotal == 0)
      return;

    peersRemaining = peersTotal;
    needsAdjustment = 1;

    if (host -> outgoingBandwidth == 0)
      bandwidth = ~0;
    else
      bandwidth = (host -> outgoingBandwidth * elapsedTime) / 1000;

    while (peersRemaining > 0 && needsAdjustment != 0)
    {
        needsAdjustment = 0;
        
        if (dataTotal < bandwidth)
          throttle = ENET_PEER_PACKET_THROTTLE_SCALE;
        else
          throttle = (bandwidth * ENET_PEER_PACKET_THROTTLE_SCALE) / dataTotal;

        for (peer = host -> peers;
             peer < & host -> peers [host -> peerCount];
             ++ peer)
        {
            uint32 peerBandwidth;
            
            if (peer -> state != ENET_PEER_STATE_CONNECTED ||
                peer -> incomingBandwidth == 0 ||
                peer -> outgoingBandwidthThrottleEpoch == timeCurrent)
              continue;

            peerBandwidth = (peer -> incomingBandwidth * elapsedTime) / 1000;
            if ((throttle * peer -> outgoingDataTotal) / ENET_PEER_PACKET_THROTTLE_SCALE <= peerBandwidth)
              continue;

            peer -> packetThrottleLimit = (peerBandwidth * 
                                            ENET_PEER_PACKET_THROTTLE_SCALE) / peer -> outgoingDataTotal;
            
            if (peer -> packetThrottleLimit == 0)
              peer -> packetThrottleLimit = 1;
            
            if (peer -> packetThrottle > peer -> packetThrottleLimit)
              peer -> packetThrottle = peer -> packetThrottleLimit;

            peer -> outgoingBandwidthThrottleEpoch = timeCurrent;

            
            needsAdjustment = 1;
            -- peersRemaining;
            bandwidth -= peerBandwidth;
            dataTotal -= peerBandwidth;
        }
    }

    if (peersRemaining > 0)
    for (peer = host -> peers;
         peer < & host -> peers [host -> peerCount];
         ++ peer)
    {
        if (peer -> state != ENET_PEER_STATE_CONNECTED ||
            peer -> outgoingBandwidthThrottleEpoch == timeCurrent)
          continue;

        peer -> packetThrottleLimit = throttle;

        if (peer -> packetThrottle > peer -> packetThrottleLimit)
          peer -> packetThrottle = peer -> packetThrottleLimit;
    }
    
    if (host -> recalculateBandwidthLimits)
    {
       host -> recalculateBandwidthLimits = 0;

       peersRemaining = peersTotal;
       bandwidth = host -> incomingBandwidth;
       needsAdjustment = 1;

       if (bandwidth == 0)
         bandwidthLimit = 0;
       else
       while (peersRemaining > 0 && needsAdjustment != 0)
       {
           needsAdjustment = 0;
           bandwidthLimit = bandwidth / peersRemaining;

           for (peer = host -> peers;
                peer < & host -> peers [host -> peerCount];
                ++ peer)
           {
               if (peer -> state != ENET_PEER_STATE_CONNECTED ||
                   peer -> incomingBandwidthThrottleEpoch == timeCurrent)
                 continue;

               if (peer -> outgoingBandwidth > 0 &&
                   bandwidthLimit > peer -> outgoingBandwidth)
                 continue;

               peer -> incomingBandwidthThrottleEpoch = timeCurrent;
 
               needsAdjustment = 1;
               -- peersRemaining;
               bandwidth -= peer -> outgoingBandwidth;
           }
       }

       for (peer = host -> peers;
            peer < & host -> peers [host -> peerCount];
            ++ peer)
       {
           if (peer -> state != ENET_PEER_STATE_CONNECTED)
             continue;

           command.header.command = ENET_PROTOCOL_COMMAND_BANDWIDTH_LIMIT;
           command.header.channelID = 0xFF;
           command.header.flags = 0;
           command.header.commandLength = sizeof (ENetProtocolBandwidthLimit);
           command.bandwidthLimit.outgoingBandwidth = ENET_HOST_TO_NET_32 (host -> outgoingBandwidth);

           if (peer -> incomingBandwidthThrottleEpoch == timeCurrent)
             command.bandwidthLimit.incomingBandwidth = ENET_HOST_TO_NET_32 (peer -> outgoingBandwidth);
           else
             command.bandwidthLimit.incomingBandwidth = ENET_HOST_TO_NET_32 (bandwidthLimit);

           enet_peer_queue_outgoing_command (peer, & command, NULL, 0, 0);
       } 
    }

    host -> bandwidthThrottleEpoch = timeCurrent;

    for (peer = host -> peers;
         peer < & host -> peers [host -> peerCount];
         ++ peer)
    {
        peer -> incomingDataTotal = 0;
        peer -> outgoingDataTotal = 0;
    }
}
    
