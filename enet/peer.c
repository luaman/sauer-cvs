#include <enet/memory.h>
#include <enet/enet.h>

void
enet_peer_throttle_configure (ENetPeer * peer, uint32 interval, uint32 acceleration, uint32 deceleration)
{
    ENetProtocol command;

    peer -> packetThrottleInterval = interval;
    peer -> packetThrottleAcceleration = acceleration;
    peer -> packetThrottleDeceleration = deceleration;

    command.header.command = ENET_PROTOCOL_COMMAND_THROTTLE_CONFIGURE;
    command.header.channelID = 0xFF;
    command.header.flags = 0;
    command.header.commandLength = sizeof (ENetProtocolThrottleConfigure);

    command.throttleConfigure.packetThrottleInterval = ENET_HOST_TO_NET_32 (interval);
    command.throttleConfigure.packetThrottleAcceleration = ENET_HOST_TO_NET_32 (acceleration);
    command.throttleConfigure.packetThrottleDeceleration = ENET_HOST_TO_NET_32 (deceleration);

    enet_peer_queue_outgoing_command (peer, & command, NULL, 0, 0);
}

int
enet_peer_throttle (ENetPeer * peer, uint32 rtt)
{
    if (peer -> bestRoundTripTime <= peer -> roundTripTimeVariance)
    {
        peer -> packetThrottle = peer -> packetThrottleLimit;
    }
    else
    if (rtt < peer -> bestRoundTripTime - peer -> roundTripTimeVariance)
    {
        peer -> packetThrottle += peer -> packetThrottleAcceleration;

        if (peer -> packetThrottle > peer -> packetThrottleLimit)
          peer -> packetThrottle = peer -> packetThrottleLimit;

        return 1;
    }
    else
    if (rtt > peer -> bestRoundTripTime + peer -> roundTripTimeVariance)
    {
        if (peer -> packetThrottle > peer -> packetThrottleDeceleration)
          peer -> packetThrottle -= peer -> packetThrottleDeceleration;
        else
          peer -> packetThrottle = 0;

        return -1;
    }

    return 0;
}

int
enet_peer_send (ENetPeer * peer, uint8 channelID, ENetPacket * packet)
{
   ENetChannel * channel = & peer -> channels [channelID];
   ENetProtocol command;
   size_t fragmentLength;

   if (peer -> state != ENET_PEER_STATE_CONNECTED)
     return -1;

   fragmentLength = peer -> packetSize - sizeof (ENetProtocolHeader) - sizeof (ENetProtocolSendFragment);

   if (packet -> dataLength > fragmentLength)
   {
      uint32 fragmentCount = ENET_HOST_TO_NET_32 ((packet -> dataLength + fragmentLength - 1) / fragmentLength),
             startSequenceNumber = ENET_HOST_TO_NET_32 (channel -> outgoingReliableSequenceNumber + 1),
             fragmentNumber,
             fragmentOffset;

      packet -> flags |= ENET_PACKET_FLAG_RELIABLE;

      for (fragmentNumber = 0,
             fragmentOffset = 0;
           fragmentOffset < packet -> dataLength;
           ++ fragmentNumber,
             fragmentOffset += fragmentLength)
      {
         command.header.command = ENET_PROTOCOL_COMMAND_SEND_FRAGMENT;
         command.header.channelID = channelID;
         command.header.flags = ENET_PROTOCOL_FLAG_ACKNOWLEDGE;
         command.header.commandLength = sizeof (ENetProtocolSendFragment);
         command.sendFragment.startSequenceNumber = startSequenceNumber;
         command.sendFragment.fragmentCount = fragmentCount;
         command.sendFragment.fragmentNumber = ENET_HOST_TO_NET_32 (fragmentNumber);
         command.sendFragment.totalLength = ENET_HOST_TO_NET_32 (packet -> dataLength);
         command.sendFragment.fragmentOffset = ENET_NET_TO_HOST_32 (fragmentOffset);

         if (packet -> dataLength - fragmentOffset < fragmentLength)
           fragmentLength = packet -> dataLength - fragmentOffset;

         enet_peer_queue_outgoing_command (peer, & command, packet, fragmentOffset, fragmentLength);
      }

      return 0;
   }

   command.header.channelID = channelID;

   if (packet -> flags & ENET_PACKET_FLAG_RELIABLE)
   {
      command.header.command = ENET_PROTOCOL_COMMAND_SEND_RELIABLE;
      command.header.flags = ENET_PROTOCOL_FLAG_ACKNOWLEDGE;
      command.header.commandLength = sizeof (ENetProtocolSendReliable);
   }
   else
   {
      command.header.command = ENET_PROTOCOL_COMMAND_SEND_UNRELIABLE;
      command.header.flags = 0;
      command.header.commandLength = sizeof (ENetProtocolSendUnreliable);
      command.sendUnreliable.unreliableSequenceNumber = ENET_HOST_TO_NET_32 (channel -> outgoingUnreliableSequenceNumber + 1);
   }

   enet_peer_queue_outgoing_command (peer, & command, packet, 0, packet -> dataLength);

   return 0;
}

ENetPacket *
enet_peer_receive (ENetPeer * peer, uint8 channelID)
{
   ENetChannel * channel = & peer -> channels [channelID];
   ENetIncomingCommand * incomingCommand = NULL;
   ENetPacket * packet;

   if (enet_list_empty (& channel -> incomingUnreliableCommands) == 0)
   {
      incomingCommand = (ENetIncomingCommand *) enet_list_front (& channel -> incomingUnreliableCommands);

      if (incomingCommand -> reliableSequenceNumber > channel -> incomingReliableSequenceNumber)
        incomingCommand = NULL;
      else
        channel -> incomingUnreliableSequenceNumber = incomingCommand -> unreliableSequenceNumber;
   }

   if (incomingCommand == NULL &&
       enet_list_empty (& channel -> incomingReliableCommands) == 0)
   {
      do
      {
        incomingCommand = (ENetIncomingCommand *) enet_list_front (& channel -> incomingReliableCommands);

        if (incomingCommand -> fragmentsRemaining > 0 ||
            incomingCommand -> reliableSequenceNumber > channel -> incomingReliableSequenceNumber + 1)
          return NULL;

        if (incomingCommand -> reliableSequenceNumber <= channel -> incomingReliableSequenceNumber)
        {
           -- incomingCommand -> packet -> referenceCount;

           if (incomingCommand -> packet -> referenceCount == 0)
             enet_packet_destroy (incomingCommand -> packet);

           if (incomingCommand -> fragments != NULL)
             enet_free (incomingCommand -> fragments);

           enet_list_remove (& incomingCommand -> incomingCommandList);

           enet_free (incomingCommand);

           incomingCommand = NULL;
        }
      } while (incomingCommand == NULL &&
               enet_list_empty (& channel -> incomingReliableCommands) == 0);

      if (incomingCommand == NULL)
        return NULL;

      channel -> incomingReliableSequenceNumber = incomingCommand -> reliableSequenceNumber;

      if (incomingCommand -> fragmentCount > 0)
        channel -> incomingReliableSequenceNumber += incomingCommand -> fragmentCount - 1;
   }

   if (incomingCommand == NULL)
     return NULL;

   enet_list_remove (& incomingCommand -> incomingCommandList);

   packet = incomingCommand -> packet;

   -- packet -> referenceCount;

   if (incomingCommand -> fragments != NULL)
     enet_free (incomingCommand -> fragments);

   enet_free (incomingCommand);

   return packet;
}

static void
enet_peer_reset_outgoing_commands (ENetList * queue)
{
    ENetOutgoingCommand * outgoingCommand;

    while (enet_list_empty (queue) == 0)
    {
       outgoingCommand = (ENetOutgoingCommand *) enet_list_remove (enet_list_begin (queue));

       if (outgoingCommand -> packet != NULL)
       {
          -- outgoingCommand -> packet -> referenceCount;

          if (outgoingCommand -> packet -> referenceCount == 0)
            enet_packet_destroy (outgoingCommand -> packet);
       }

       enet_free (outgoingCommand);
    }
}

static void
enet_peer_reset_incoming_commands (ENetList * queue)
{
    ENetIncomingCommand * incomingCommand;

    while (enet_list_empty (queue) == 0)
    {
       incomingCommand = (ENetIncomingCommand *) enet_list_remove (enet_list_begin (queue));

       if (incomingCommand -> packet != NULL)
       {
          -- incomingCommand -> packet -> referenceCount;

          if (incomingCommand -> packet -> referenceCount == 0)
            enet_packet_destroy (incomingCommand -> packet);
       }

       enet_free (incomingCommand);
    }
}

void
enet_peer_reset (ENetPeer * peer)
{
    ENetChannel * channel;

    peer -> outgoingPeerID = 0xFFFF;
    peer -> challenge = 0;

    peer -> address.host = ENET_HOST_ANY;
    peer -> address.port = 0;

    peer -> state = ENET_PEER_STATE_DISCONNECTED;

    peer -> incomingBandwidth = 0;
    peer -> outgoingBandwidth = 0;
    peer -> incomingBandwidthThrottleEpoch = 0;
    peer -> outgoingBandwidthThrottleEpoch = 0;
    peer -> incomingDataTotal = 0;
    peer -> outgoingDataTotal = 0;
    peer -> lastSendTime = 0;
    peer -> lastReceiveTime = 0;
    peer -> nextTimeout = 0;
    peer -> packetLossEpoch = 0;
    peer -> packetsSent = 0;
    peer -> packetsLost = 0;
    peer -> packetLoss = 0;
    peer -> packetLossVariance = 0;
    peer -> packetThrottle = ENET_PEER_DEFAULT_PACKET_THROTTLE;
    peer -> packetThrottleLimit = ENET_PEER_PACKET_THROTTLE_SCALE;
    peer -> packetThrottleCounter = 0;
    peer -> packetThrottleEpoch = 0;
    peer -> packetThrottleAcceleration = ENET_PEER_PACKET_THROTTLE_ACCELERATION;
    peer -> packetThrottleDeceleration = ENET_PEER_PACKET_THROTTLE_DECELERATION;
    peer -> packetThrottleInterval = ENET_PEER_PACKET_THROTTLE_INTERVAL;
    peer -> bestRoundTripTime = ENET_PEER_DEFAULT_ROUND_TRIP_TIME;
    peer -> roundTripTime = ENET_PEER_DEFAULT_ROUND_TRIP_TIME;
    peer -> roundTripTimeVariance = 0;
    peer -> packetSize = ENET_PROTOCOL_MINIMUM_PACKET_SIZE;
    peer -> reliableDataInTransit = 0;
    peer -> outgoingReliableSequenceNumber = 0;

    if (peer -> host -> outgoingBandwidth == 0)
      peer -> windowSize = ENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;
    else
      peer -> windowSize = (peer -> host -> outgoingBandwidth / 
                             ENET_PEER_WINDOW_SIZE_SCALE) * ENET_PROTOCOL_MINIMUM_WINDOW_SIZE;

    if (peer -> windowSize < ENET_PROTOCOL_MINIMUM_WINDOW_SIZE)
      peer -> windowSize = ENET_PROTOCOL_MINIMUM_WINDOW_SIZE;
    else
    if (peer -> windowSize > ENET_PROTOCOL_MAXIMUM_WINDOW_SIZE)
      peer -> windowSize = ENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;
    
    while (enet_list_empty (& peer -> acknowledgements) == 0)
      enet_free (enet_list_remove (enet_list_begin (& peer -> acknowledgements)));

    enet_peer_reset_outgoing_commands (& peer -> sentReliableCommands);
    enet_peer_reset_outgoing_commands (& peer -> sentUnreliableCommands);
    enet_peer_reset_outgoing_commands (& peer -> outgoingReliableCommands);
    enet_peer_reset_outgoing_commands (& peer -> outgoingUnreliableCommands);

    if (peer -> channels != NULL && peer -> channelCount > 0)
    {
        for (channel = peer -> channels;
             channel < & peer -> channels [peer -> channelCount];
             ++ channel)
        {
            channel -> outgoingReliableSequenceNumber = 0;
            channel -> outgoingUnreliableSequenceNumber = 0;
            channel -> incomingReliableSequenceNumber = 0;
            channel -> incomingUnreliableSequenceNumber = 0;
             
            enet_peer_reset_incoming_commands (& channel -> incomingReliableCommands);
            enet_peer_reset_incoming_commands (& channel -> incomingUnreliableCommands);
        }

        enet_free (peer -> channels);
    }

    peer -> channels = NULL;
    peer -> channelCount = 0;
}

void
enet_peer_ping (ENetPeer * peer)
{
    ENetProtocol command;

    if (peer -> state != ENET_PEER_STATE_CONNECTED)
      return;

    command.header.command = ENET_PROTOCOL_COMMAND_PING;
    command.header.channelID = 0xFF;
    command.header.flags = ENET_PROTOCOL_FLAG_ACKNOWLEDGE;
    command.header.commandLength = sizeof (ENetProtocolPing);
   
    enet_peer_queue_outgoing_command (peer, & command, NULL, 0, 0);
}

void
enet_peer_disconnect (ENetPeer * peer)
{
    ENetProtocol command;

    if (peer -> state == ENET_PEER_STATE_DISCONNECTING ||
        peer -> state == ENET_PEER_STATE_DISCONNECTED)
      return;

    peer -> state = ENET_PEER_STATE_DISCONNECTING;

    command.header.command = ENET_PROTOCOL_COMMAND_DISCONNECT;
    command.header.channelID = 0xFF;
    command.header.flags = ENET_PROTOCOL_FLAG_ACKNOWLEDGE;
    command.header.commandLength = sizeof (ENetProtocolDisconnect);

    enet_peer_queue_outgoing_command (peer, & command, NULL, 0, 0);
}

ENetAcknowledgement *
enet_peer_queue_acknowledgement (ENetPeer * peer, const ENetProtocol * command, uint32 sentTime)
{
    ENetAcknowledgement * acknowledgement;

    peer -> outgoingDataTotal += sizeof (ENetProtocolAcknowledge);

    acknowledgement = (ENetAcknowledgement *) enet_malloc (sizeof (ENetAcknowledgement));

    acknowledgement -> sentTime = sentTime;
    acknowledgement -> command = * command;
    
    enet_list_insert (enet_list_end (& peer -> acknowledgements), acknowledgement);
    
    return acknowledgement;
}

ENetOutgoingCommand *
enet_peer_queue_outgoing_command (ENetPeer * peer, const ENetProtocol * command, ENetPacket * packet, uint32 offset, uint16 length)
{
    ENetChannel * channel = & peer -> channels [command -> header.channelID];
    ENetOutgoingCommand * outgoingCommand;

    peer -> outgoingDataTotal += command -> header.commandLength + length;

    outgoingCommand = (ENetOutgoingCommand *) enet_malloc (sizeof (ENetOutgoingCommand));

    if (command -> header.channelID == 0xFF)
    {
       ++ peer -> outgoingReliableSequenceNumber;

       outgoingCommand -> reliableSequenceNumber = peer -> outgoingReliableSequenceNumber;
       outgoingCommand -> unreliableSequenceNumber = 0;
    }
    else
    if (command -> header.flags & ENET_PROTOCOL_FLAG_ACKNOWLEDGE)
    {
       ++ channel -> outgoingReliableSequenceNumber;
       
       outgoingCommand -> reliableSequenceNumber = channel -> outgoingReliableSequenceNumber;
       outgoingCommand -> unreliableSequenceNumber = 0;
    }
    else
    {
       ++ channel -> outgoingUnreliableSequenceNumber;
        
       outgoingCommand -> reliableSequenceNumber = channel -> outgoingReliableSequenceNumber;
       outgoingCommand -> unreliableSequenceNumber = channel -> outgoingUnreliableSequenceNumber;
    }
   
    outgoingCommand -> sentTime = 0;
    outgoingCommand -> roundTripTimeout = 0;
    outgoingCommand -> roundTripTimeoutLimit = 0;
    outgoingCommand -> fragmentOffset = offset;
    outgoingCommand -> fragmentLength = length;
    outgoingCommand -> packet = packet;
    outgoingCommand -> command = * command;
    outgoingCommand -> command.header.reliableSequenceNumber = ENET_HOST_TO_NET_32 (outgoingCommand -> reliableSequenceNumber);

    if (packet != NULL)
      ++ packet -> referenceCount;

    if (command -> header.flags & ENET_PROTOCOL_FLAG_ACKNOWLEDGE)
      enet_list_insert (enet_list_end (& peer -> outgoingReliableCommands), outgoingCommand);
    else
      enet_list_insert (enet_list_end (& peer -> outgoingUnreliableCommands), outgoingCommand);

    return outgoingCommand;
}

ENetIncomingCommand *
enet_peer_queue_incoming_command (ENetPeer * peer, const ENetProtocol * command, ENetPacket * packet, uint32 fragmentCount)
{
    ENetChannel * channel = & peer -> channels [command -> header.channelID];
    uint32 unreliableSequenceNumber = 0;
    ENetIncomingCommand * incomingCommand;
    ENetListIterator currentCommand;

    if (command -> header.command == ENET_PROTOCOL_COMMAND_SEND_UNRELIABLE)
      unreliableSequenceNumber = ENET_NET_TO_HOST_32 (command -> sendUnreliable.unreliableSequenceNumber);

    if (unreliableSequenceNumber == 0)
    {
       for (currentCommand = enet_list_previous (enet_list_end (& channel -> incomingReliableCommands));
            currentCommand != enet_list_end (& channel -> incomingReliableCommands);
            currentCommand = enet_list_previous (currentCommand))
       {
          incomingCommand = (ENetIncomingCommand *) currentCommand;

          if (incomingCommand -> reliableSequenceNumber <= command -> header.reliableSequenceNumber)
          {
             if (incomingCommand -> reliableSequenceNumber < command -> header.reliableSequenceNumber)
               break;

             goto freePacket;
          }
       }
    }
    else
    {
       if (command -> header.reliableSequenceNumber < channel -> incomingReliableSequenceNumber)
         goto freePacket;

       if (unreliableSequenceNumber <= channel -> incomingUnreliableSequenceNumber)
         goto freePacket;

       for (currentCommand = enet_list_previous (enet_list_end (& channel -> incomingUnreliableCommands));
            currentCommand != enet_list_end (& channel -> incomingUnreliableCommands);
            currentCommand = enet_list_previous (currentCommand))
       {
          incomingCommand = (ENetIncomingCommand *) currentCommand;

          if (incomingCommand -> unreliableSequenceNumber <= unreliableSequenceNumber)
          {
             if (incomingCommand -> unreliableSequenceNumber < unreliableSequenceNumber)
               break;

             goto freePacket;
          }
       }
    }
        
    incomingCommand = (ENetIncomingCommand *) enet_malloc (sizeof (ENetIncomingCommand));

    incomingCommand -> reliableSequenceNumber = command -> header.reliableSequenceNumber;
    incomingCommand -> unreliableSequenceNumber = unreliableSequenceNumber;
    incomingCommand -> command = * command;
    incomingCommand -> fragmentCount = fragmentCount;
    incomingCommand -> fragmentsRemaining = fragmentCount;
    incomingCommand -> packet = packet;

    if (fragmentCount > 0)
      incomingCommand -> fragments = (uint32 *) enet_calloc ((fragmentCount + 31) / 32, sizeof (uint32));
    else
      incomingCommand -> fragments = NULL;

    if (packet != NULL)
      ++ packet -> referenceCount;

    enet_list_insert (enet_list_next (currentCommand), incomingCommand);

    return incomingCommand;

freePacket:
    if (packet != NULL)
    {
       if (packet -> referenceCount == 0)
         enet_packet_destroy (packet);
    }

    return NULL;
}

