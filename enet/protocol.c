#include <stdio.h>
#include <enet/memory.h>
#include <enet/time.h>
#include <enet/enet.h>

static uint32 timeCurrent;

static int
enet_protocol_dispatch_incoming_commands (ENetHost * host, ENetEvent * event)
{
    ENetPeer * currentPeer = host -> lastServicedPeer;
    ENetChannel * channel;

    do
    {
       ++ currentPeer;
       
       if (currentPeer >= & host -> peers [host -> peerCount])
         currentPeer = host -> peers;

       if (currentPeer -> state == ENET_PEER_STATE_DISCONNECTED)
         continue;

       if (currentPeer -> state == ENET_PEER_STATE_ZOMBIE)
       {
           host -> recalculateBandwidthLimits = 1;

           event -> type = ENET_EVENT_TYPE_DISCONNECT;
           event -> peer = currentPeer;

           enet_peer_reset (currentPeer);

           host -> lastServicedPeer = currentPeer;

           return 1;
       }

       for (channel = currentPeer -> channels;
            channel < & currentPeer -> channels [currentPeer -> channelCount];
            ++ channel)
       {
           if (enet_list_empty (& channel -> incomingReliableCommands) &&
               enet_list_empty (& channel -> incomingUnreliableCommands))
             continue;

           event -> packet = enet_peer_receive (currentPeer, channel - currentPeer -> channels);
           if (event -> packet == NULL)
             continue;
             
           
           event -> type = ENET_EVENT_TYPE_RECEIVE;
           event -> peer = currentPeer;
           event -> channelID = (uint8) (channel - currentPeer -> channels);

           host -> lastServicedPeer = currentPeer;

           return 1;
       }
    } while (currentPeer != host -> lastServicedPeer);

    return 0;
}

static void
enet_protocol_remove_sent_unreliable_commands (ENetPeer * peer)
{
    ENetOutgoingCommand * outgoingCommand;

    while (enet_list_empty (& peer -> sentUnreliableCommands) == 0)
    {
        outgoingCommand = (ENetOutgoingCommand *) enet_list_front (& peer -> sentUnreliableCommands);
        
        enet_list_remove (& outgoingCommand -> outgoingCommandList);

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
enet_protocol_remove_sent_reliable_command (ENetPeer * peer, uint32 reliableSequenceNumber, uint8 channelID)
{
    ENetOutgoingCommand * outgoingCommand;
    ENetListIterator currentCommand;
   
    for (currentCommand = enet_list_begin (& peer -> sentReliableCommands);
         currentCommand != enet_list_end (& peer -> sentReliableCommands);
         currentCommand = enet_list_next (currentCommand))
    {
       outgoingCommand = (ENetOutgoingCommand *) currentCommand;
        
       if (outgoingCommand -> reliableSequenceNumber == reliableSequenceNumber &&
           outgoingCommand -> command.header.channelID == channelID)
         break;
    }

    if (currentCommand == enet_list_end (& peer -> sentReliableCommands))
      return;

    enet_list_remove (& outgoingCommand -> outgoingCommandList);

    if (outgoingCommand -> packet != NULL)
    {
       peer -> reliableDataInTransit -= outgoingCommand -> fragmentLength;

       -- outgoingCommand -> packet -> referenceCount;

       if (outgoingCommand -> packet -> referenceCount == 0)
         enet_packet_destroy (outgoingCommand -> packet);
    }

    enet_free (outgoingCommand);

    if (enet_list_empty (& peer -> sentReliableCommands))
      return;
    
    outgoingCommand = (ENetOutgoingCommand *) enet_list_front (& peer -> sentReliableCommands);
    
    peer -> nextTimeout = outgoingCommand -> sentTime + outgoingCommand -> roundTripTimeout;
} 

static ENetPeer *
enet_protocol_handle_connect (ENetHost * host, const ENetProtocolHeader * header, const ENetProtocol * command)
{
    uint16 packetSize;
    uint32 windowSize;
    ENetChannel * channel;
    size_t channelCount;
    ENetPeer * currentPeer;
    ENetProtocol verifyCommand;

    if (command -> header.commandLength < sizeof (ENetProtocolConnect))
      return NULL;

    channelCount = ENET_NET_TO_HOST_32 (command -> connect.channelCount);

    if (channelCount < ENET_PROTOCOL_MINIMUM_CHANNEL_COUNT ||
        channelCount > ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT)
      return NULL;

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
    currentPeer -> challenge = header -> challenge;
    currentPeer -> address = host -> receivedAddress;
    currentPeer -> outgoingPeerID = ENET_NET_TO_HOST_16 (command -> connect.outgoingPeerID);
    currentPeer -> incomingBandwidth = ENET_NET_TO_HOST_32 (command -> connect.incomingBandwidth);
    currentPeer -> outgoingBandwidth = ENET_NET_TO_HOST_32 (command -> connect.outgoingBandwidth);
    currentPeer -> packetThrottleInterval = ENET_NET_TO_HOST_32 (command -> connect.packetThrottleInterval);
    currentPeer -> packetThrottleAcceleration = ENET_NET_TO_HOST_32 (command -> connect.packetThrottleAcceleration);
    currentPeer -> packetThrottleDeceleration = ENET_NET_TO_HOST_32 (command -> connect.packetThrottleDeceleration);
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

    packetSize = ENET_NET_TO_HOST_16 (command -> connect.packetSize);

    if (packetSize < ENET_PROTOCOL_MINIMUM_PACKET_SIZE)
      packetSize = ENET_PROTOCOL_MINIMUM_PACKET_SIZE;
    else
    if (packetSize > ENET_PROTOCOL_MAXIMUM_PACKET_SIZE)
      packetSize = ENET_PROTOCOL_MAXIMUM_PACKET_SIZE;

    currentPeer -> packetSize = packetSize;

    windowSize = ENET_NET_TO_HOST_32 (command -> connect.windowSize);
   
    if (windowSize < ENET_PROTOCOL_MINIMUM_WINDOW_SIZE)
      windowSize = ENET_PROTOCOL_MINIMUM_WINDOW_SIZE;
    else
    if (windowSize > ENET_PROTOCOL_MAXIMUM_WINDOW_SIZE)
      windowSize = ENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;

    if (windowSize < currentPeer -> windowSize)
      currentPeer -> windowSize = windowSize;

    verifyCommand.header.command = ENET_PROTOCOL_COMMAND_VERIFY_CONNECT;
    verifyCommand.header.channelID = 0xFF;
    verifyCommand.header.flags = ENET_PROTOCOL_FLAG_ACKNOWLEDGE;
    verifyCommand.header.commandLength = sizeof (ENetProtocolVerifyConnect);
    verifyCommand.verifyConnect.outgoingPeerID = ENET_HOST_TO_NET_16 (currentPeer -> incomingPeerID);
    verifyCommand.verifyConnect.packetSize = ENET_HOST_TO_NET_16 (currentPeer -> packetSize);
    verifyCommand.verifyConnect.windowSize = ENET_HOST_TO_NET_32 (currentPeer -> windowSize);
    verifyCommand.verifyConnect.channelCount = ENET_HOST_TO_NET_32 (channelCount);
    verifyCommand.verifyConnect.incomingBandwidth = ENET_HOST_TO_NET_32 (host -> incomingBandwidth);
    verifyCommand.verifyConnect.outgoingBandwidth = ENET_HOST_TO_NET_32 (host -> outgoingBandwidth);
    verifyCommand.verifyConnect.packetThrottleInterval = ENET_HOST_TO_NET_32 (currentPeer -> packetThrottleInterval);
    verifyCommand.verifyConnect.packetThrottleAcceleration = ENET_HOST_TO_NET_32 (currentPeer -> packetThrottleAcceleration);
    verifyCommand.verifyConnect.packetThrottleDeceleration = ENET_HOST_TO_NET_32 (currentPeer -> packetThrottleDeceleration);

    enet_peer_queue_outgoing_command (currentPeer, & verifyCommand, NULL, 0, 0);

    return currentPeer;
}

static void
enet_protocol_handle_send_reliable (ENetHost * host, ENetPeer * peer, const ENetProtocol * command)
{
    ENetPacket * packet;

    if (command -> header.commandLength <= sizeof (ENetProtocolSendReliable))
      return;

    packet = enet_packet_create ((const uint8 *) command + sizeof (ENetProtocolSendReliable),
                                 command -> header.commandLength - sizeof (ENetProtocolSendReliable),
                                 ENET_PACKET_FLAG_RELIABLE);

    enet_peer_queue_incoming_command (peer, command, packet, 0);
}

static void
enet_protocol_handle_send_unreliable (ENetHost * host, ENetPeer * peer, const ENetProtocol * command)
{
    ENetPacket * packet;

    if (command -> header.commandLength <= sizeof (ENetProtocolSendUnreliable))
      return;

    packet = enet_packet_create ((const uint8 *) command + sizeof (ENetProtocolSendUnreliable),
                                 command -> header.commandLength - sizeof (ENetProtocolSendUnreliable),
                                 0);

    enet_peer_queue_incoming_command (peer, command, packet, 0);
}

static void
enet_protocol_handle_send_fragment (ENetHost * host, ENetPeer * peer, const ENetProtocol * command)
{
    uint32 fragmentNumber,
           fragmentCount,
           fragmentOffset,
           fragmentLength,
           startSequenceNumber,
           totalLength;
    ENetChannel * channel;
    ENetListIterator currentCommand;
    ENetIncomingCommand * startCommand;

    if (command -> header.commandLength <= sizeof (ENetProtocolSendFragment))
      return;

    startSequenceNumber = ENET_NET_TO_HOST_32 (command -> sendFragment.startSequenceNumber);
    fragmentNumber = ENET_NET_TO_HOST_32 (command -> sendFragment.fragmentNumber);
    fragmentCount = ENET_NET_TO_HOST_32 (command -> sendFragment.fragmentCount);
    fragmentOffset = ENET_NET_TO_HOST_32 (command -> sendFragment.fragmentOffset);
    fragmentLength = command -> header.commandLength - sizeof (ENetProtocolSendFragment);
    totalLength = ENET_NET_TO_HOST_32 (command -> sendFragment.totalLength);

    if (fragmentOffset >= totalLength ||
        fragmentOffset + fragmentLength > totalLength ||
        fragmentNumber >= fragmentCount)
      return;
 
    channel = & peer -> channels [command -> header.channelID];

    for (currentCommand = enet_list_previous (enet_list_end (& channel -> incomingReliableCommands));
         currentCommand != enet_list_end (& channel -> incomingReliableCommands);
         currentCommand = enet_list_previous (currentCommand))
    {
       startCommand = (ENetIncomingCommand *) currentCommand;

       if (startCommand -> reliableSequenceNumber == startSequenceNumber)
         break;
    }
 
    if (currentCommand == enet_list_end (& channel -> incomingReliableCommands))
    {
       startCommand = enet_peer_queue_incoming_command (peer, 
                                                        command, 
                                                        enet_packet_create (NULL, totalLength, ENET_PACKET_FLAG_RELIABLE),
                                                        fragmentCount);
    }
    else
    if (totalLength != startCommand -> packet -> dataLength ||
        fragmentCount != startCommand -> fragmentCount)
      return;
    
    if ((startCommand -> fragments [fragmentNumber / 32] & (1 << fragmentNumber)) == 0)
      -- startCommand -> fragmentsRemaining;

    startCommand -> fragments [fragmentNumber / 32] |= (1 << fragmentNumber);

    if (fragmentOffset + fragmentLength > startCommand -> packet -> dataLength)
      fragmentLength = startCommand -> packet -> dataLength - fragmentOffset;

    memcpy (startCommand -> packet -> data + fragmentOffset,
            (uint8 *) command + sizeof (ENetProtocolSendFragment),
            fragmentLength);
}

static void
enet_protocol_handle_ping (ENetHost * host, ENetPeer * peer, const ENetProtocol * command)
{
    if (command -> header.commandLength < sizeof (ENetProtocolPing))
      return;
}

static void
enet_protocol_handle_bandwidth_limit (ENetHost * host, ENetPeer * peer, const ENetProtocol * command)
{
    if (command -> header.commandLength < sizeof (ENetProtocolBandwidthLimit))
      return;

    peer -> incomingBandwidth = ENET_NET_TO_HOST_32 (command -> bandwidthLimit.incomingBandwidth);
    peer -> outgoingBandwidth = ENET_NET_TO_HOST_32 (command -> bandwidthLimit.outgoingBandwidth);
}

static void
enet_protocol_handle_throttle_configure (ENetHost * host, ENetPeer * peer, const ENetProtocol * command)
{
    if (command -> header.commandLength < sizeof (ENetProtocolThrottleConfigure))
      return;

    peer -> packetThrottleInterval = ENET_NET_TO_HOST_32 (command -> throttleConfigure.packetThrottleInterval);
    peer -> packetThrottleAcceleration = ENET_NET_TO_HOST_32 (command -> throttleConfigure.packetThrottleAcceleration);
    peer -> packetThrottleDeceleration = ENET_NET_TO_HOST_32 (command -> throttleConfigure.packetThrottleDeceleration);
}

static void
enet_protocol_handle_disconnect (ENetHost * host, ENetPeer * peer, const ENetProtocol * command)
{
    if (command -> header.commandLength < sizeof (ENetProtocolDisconnect))
      return;

    peer -> state = ENET_PEER_STATE_ACKNOWLEDGING_DISCONNECTION;
}

static int
enet_protocol_handle_acknowledge (ENetHost * host, ENetEvent * event, ENetPeer * peer, const ENetProtocol * command)
{
    uint32 roundTripTime,
           receivedSentTime,
           receivedReliableSequenceNumber;
           
    if (command -> header.commandLength < sizeof (ENetProtocolAcknowledge))
      return 0;

    receivedSentTime = ENET_NET_TO_HOST_32 (command -> acknowledge.receivedSentTime);

    if (ENET_TIME_LESS (timeCurrent, receivedSentTime))
      return 0;

    peer -> lastReceiveTime = timeCurrent;

    roundTripTime = ENET_TIME_DIFFERENCE (timeCurrent, receivedSentTime);

    peer -> roundTripTimeVariance -= peer -> roundTripTimeVariance / 4;

    if (roundTripTime >= peer -> roundTripTime)
    {
       peer -> roundTripTime += (roundTripTime - peer -> roundTripTime) / 8;
       peer -> roundTripTimeVariance += (roundTripTime - peer -> roundTripTime) / 4;
    }
    else
    {
       peer -> roundTripTime -= (peer -> roundTripTime - roundTripTime) / 8;
       peer -> roundTripTimeVariance += (peer -> roundTripTime - roundTripTime) / 4;
    }

    if (peer -> packetThrottleEpoch == 0 ||
        ENET_TIME_DIFFERENCE(timeCurrent, peer -> packetThrottleEpoch) >= peer -> packetThrottleInterval)
    {
        peer -> bestRoundTripTime = peer -> roundTripTime;
        peer -> packetThrottleEpoch = timeCurrent;
    }
    else
    if (peer -> roundTripTime < peer -> bestRoundTripTime)
      peer -> bestRoundTripTime = peer -> roundTripTime;

    enet_peer_throttle (peer, roundTripTime);
         
    receivedReliableSequenceNumber = ENET_NET_TO_HOST_32 (command -> acknowledge.receivedReliableSequenceNumber);

    enet_protocol_remove_sent_reliable_command (peer, receivedReliableSequenceNumber, command -> header.channelID);

    switch (peer -> state)
    {
    case ENET_PEER_STATE_CONNECTING:
       host -> recalculateBandwidthLimits = 1;

       peer -> state = ENET_PEER_STATE_CONNECTED;

       event -> type = ENET_EVENT_TYPE_CONNECT;
       event -> peer = peer;

       return 1;

    case ENET_PEER_STATE_DISCONNECTING:
       if (enet_list_empty (& peer -> sentReliableCommands) == 0)
         return 0;

       host -> recalculateBandwidthLimits = 1;

       event -> type = ENET_EVENT_TYPE_DISCONNECT;
       event -> peer = peer;

       enet_peer_reset (peer);

       return 1;

    default:
       break;
    }
   
    return 0;
}

static void
enet_protocol_handle_verify_connect (ENetHost * host, ENetPeer * peer, const ENetProtocol * command)
{
    uint16 packetSize;
    uint32 windowSize;

    if (command -> header.commandLength < sizeof (ENetProtocolVerifyConnect))
      return;

    if (ENET_NET_TO_HOST_32 (command -> verifyConnect.channelCount) != peer -> channelCount ||
        ENET_NET_TO_HOST_32 (command -> verifyConnect.packetThrottleInterval) != peer -> packetThrottleInterval ||
        ENET_NET_TO_HOST_32 (command -> verifyConnect.packetThrottleAcceleration) != peer -> packetThrottleAcceleration ||
        ENET_NET_TO_HOST_32 (command -> verifyConnect.packetThrottleDeceleration) != peer -> packetThrottleDeceleration)
    {
        peer -> state = ENET_PEER_STATE_ZOMBIE;

        return;
    }

    peer -> outgoingPeerID = ENET_NET_TO_HOST_16 (command -> verifyConnect.outgoingPeerID);

    packetSize = ENET_NET_TO_HOST_16 (command -> verifyConnect.packetSize);

    if (packetSize < ENET_PROTOCOL_MINIMUM_PACKET_SIZE)
      packetSize = ENET_PROTOCOL_MINIMUM_PACKET_SIZE;
   
    if (packetSize > ENET_PROTOCOL_MAXIMUM_PACKET_SIZE)
      packetSize = ENET_PROTOCOL_MAXIMUM_PACKET_SIZE;

    if (packetSize < peer -> packetSize)
      peer -> packetSize = packetSize;

    windowSize = ENET_NET_TO_HOST_32 (command -> verifyConnect.windowSize);

    if (windowSize < ENET_PROTOCOL_MINIMUM_WINDOW_SIZE)
      windowSize = ENET_PROTOCOL_MINIMUM_WINDOW_SIZE;

    if (windowSize > ENET_PROTOCOL_MAXIMUM_WINDOW_SIZE)
      windowSize = ENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;

    if (windowSize < peer -> windowSize)
      peer -> windowSize = windowSize;

    peer -> incomingBandwidth = ENET_NET_TO_HOST_32 (command -> verifyConnect.incomingBandwidth);
    peer -> outgoingBandwidth = ENET_NET_TO_HOST_32 (command -> verifyConnect.outgoingBandwidth);
}

static int
enet_protocol_handle_incoming_commands (ENetHost * host, ENetEvent * event)
{
    ENetProtocolHeader * header;
    ENetProtocol * command;
    ENetPeer * peer;
    uint8 * currentData;
    size_t commandCount;

    if (host -> receivedDataLength < sizeof (ENetProtocolHeader))
      return 0;

    header = (ENetProtocolHeader *) host -> receivedData;

    header -> peerID = ENET_NET_TO_HOST_16 (header -> peerID);
    header -> sentTime = ENET_NET_TO_HOST_32 (header -> sentTime);

    if (header -> peerID >= host -> peerCount)
      peer = NULL;
    else
    {
       peer = & host -> peers [header -> peerID];

       if (peer -> state == ENET_PEER_STATE_DISCONNECTED ||
           host -> receivedAddress.host != peer -> address.host ||
           host -> receivedAddress.port != peer -> address.port ||
           header -> challenge != peer -> challenge)
         peer = NULL;
    }

    if (peer != NULL)
      peer -> incomingDataTotal += host -> receivedDataLength;

    commandCount = header -> commandCount;
    currentData = host -> receivedData + sizeof (ENetProtocolHeader);
  
    while (commandCount > 0 &&
           currentData < & host -> receivedData [host -> receivedDataLength])
    {
       command = (ENetProtocol *) currentData;

       if (currentData + sizeof (ENetProtocolCommandHeader) > & host -> receivedData [host -> receivedDataLength])
         return 0;

       command -> header.commandLength = ENET_NET_TO_HOST_32 (command -> header.commandLength);

       if (currentData + command -> header.commandLength > & host -> receivedData [host -> receivedDataLength])
         return 0;

       -- commandCount;
       currentData += command -> header.commandLength;

       if (peer == NULL)
       {
          if (command -> header.command != ENET_PROTOCOL_COMMAND_CONNECT)
            return 0;
       }
         
       command -> header.reliableSequenceNumber = ENET_NET_TO_HOST_32 (command -> header.reliableSequenceNumber);

       switch (command -> header.command)
       {
       case ENET_PROTOCOL_COMMAND_ACKNOWLEDGE:
          enet_protocol_handle_acknowledge (host, event, peer, command);

          break;

       case ENET_PROTOCOL_COMMAND_CONNECT:
          peer = enet_protocol_handle_connect (host, header, command);

          break;

       case ENET_PROTOCOL_COMMAND_VERIFY_CONNECT:
          enet_protocol_handle_verify_connect (host, peer, command);

          break;

       case ENET_PROTOCOL_COMMAND_DISCONNECT:
          enet_protocol_handle_disconnect (host, peer, command);

          break;

       case ENET_PROTOCOL_COMMAND_PING:
          enet_protocol_handle_ping (host, peer, command);

          break;

       case ENET_PROTOCOL_COMMAND_SEND_RELIABLE:
          enet_protocol_handle_send_reliable (host, peer, command);

          break;

       case ENET_PROTOCOL_COMMAND_SEND_UNRELIABLE:
          enet_protocol_handle_send_unreliable (host, peer, command);

          break;

       case ENET_PROTOCOL_COMMAND_SEND_FRAGMENT:
          enet_protocol_handle_send_fragment (host, peer, command);

          break;

       case ENET_PROTOCOL_COMMAND_BANDWIDTH_LIMIT:
          enet_protocol_handle_bandwidth_limit (host, peer, command);

          break;

       case ENET_PROTOCOL_COMMAND_THROTTLE_CONFIGURE:
          enet_protocol_handle_throttle_configure (host, peer, command);

          break;

       default:
          break;
       }

       if (peer != NULL &&
           (command -> header.flags & ENET_PROTOCOL_FLAG_ACKNOWLEDGE) != 0)
         enet_peer_queue_acknowledgement (peer, command, header -> sentTime);        
    }

    if (event -> type != ENET_EVENT_TYPE_NONE)
      return 1;

    return 0;
}
 
static int
enet_protocol_receive_incoming_commands (ENetHost * host, ENetEvent * event)
{
    for (;;)
    {
       int receivedLength;
       ENetBuffer buffer;

       buffer.data = host -> receivedData;
       buffer.dataLength = sizeof (host -> receivedData);

       receivedLength = enet_socket_receive (host -> socket,
                                             & host -> receivedAddress,
                                             & buffer,
                                             1);

       if (receivedLength < 0)
         return -1;

       if (receivedLength == 0)
         return 0;

       host -> receivedDataLength = receivedLength;
       
       switch (enet_protocol_handle_incoming_commands (host, event))
       {
       case 1:
          return 1;
       
       case -1:
          return -1;

       default:
          break;
       }
    }

    return -1;
}

static void
enet_protocol_send_acknowledgements (ENetHost * host, ENetPeer * peer)
{
    ENetProtocol * command = & host -> commands [host -> commandCount];
    ENetBuffer * buffer = & host -> buffers [host -> bufferCount];
    ENetAcknowledgement * acknowledgement;
    ENetListIterator currentAcknowledgement;
  
    currentAcknowledgement = enet_list_begin (& peer -> acknowledgements);
         
    while (currentAcknowledgement != enet_list_end (& peer -> acknowledgements))
    {
       if (command >= & host -> commands [sizeof (host -> commands) / sizeof (ENetProtocol)] ||
           peer -> packetSize - host -> packetSize < sizeof (ENetProtocolAcknowledge))
         break;

       acknowledgement = (ENetAcknowledgement *) currentAcknowledgement;
 
       currentAcknowledgement = enet_list_next (currentAcknowledgement);

       buffer -> data = command;
       buffer -> dataLength = sizeof (ENetProtocolAcknowledge);

       host -> packetSize += buffer -> dataLength;
 
       command -> header.command = ENET_PROTOCOL_COMMAND_ACKNOWLEDGE;
       command -> header.channelID = acknowledgement -> command.header.channelID;
       command -> header.flags = 0;
       command -> header.commandLength = ENET_HOST_TO_NET_32 (sizeof (ENetProtocolAcknowledge));
       command -> acknowledge.receivedReliableSequenceNumber = ENET_HOST_TO_NET_32 (acknowledgement -> command.header.reliableSequenceNumber);
       command -> acknowledge.receivedSentTime = ENET_HOST_TO_NET_32 (acknowledgement -> sentTime);
  
       if (acknowledgement -> command.header.command == ENET_PROTOCOL_COMMAND_DISCONNECT)
         peer -> state = ENET_PEER_STATE_ZOMBIE;

       enet_list_remove (& acknowledgement -> acknowledgementList);
       enet_free (acknowledgement);

       ++ command;
       ++ buffer;
    }

    host -> commandCount = command - host -> commands;
    host -> bufferCount = buffer - host -> buffers;
}

static void
enet_protocol_send_unreliable_outgoing_commands (ENetHost * host, ENetPeer * peer)
{
    ENetProtocol * command = & host -> commands [host -> commandCount];
    ENetBuffer * buffer = & host -> buffers [host -> bufferCount];
    ENetOutgoingCommand * outgoingCommand;
    ENetListIterator currentCommand;

    currentCommand = enet_list_begin (& peer -> outgoingUnreliableCommands);
    
    while (currentCommand != enet_list_end (& peer -> outgoingUnreliableCommands))
    {
       outgoingCommand = (ENetOutgoingCommand *) currentCommand;

       if (command >= & host -> commands [sizeof (host -> commands) / sizeof (ENetProtocol)] ||
           peer -> packetSize - host -> packetSize < outgoingCommand -> command.header.commandLength ||
           (outgoingCommand -> packet != NULL &&
             peer -> packetSize - host -> packetSize < outgoingCommand -> command.header.commandLength + 
                                                         outgoingCommand -> packet -> dataLength))
         break;

       currentCommand = enet_list_next (currentCommand);

       if (outgoingCommand -> packet != NULL)
       {
          peer -> packetThrottleCounter += ENET_PEER_PACKET_THROTTLE_COUNTER;
          peer -> packetThrottleCounter %= ENET_PEER_PACKET_THROTTLE_SCALE;
          
          if (peer -> packetThrottleCounter > peer -> packetThrottle)
          {
             -- outgoingCommand -> packet -> referenceCount;

             if (outgoingCommand -> packet -> referenceCount == 0)
               enet_packet_destroy (outgoingCommand -> packet);
         
             enet_list_remove (& outgoingCommand -> outgoingCommandList);
             enet_free (outgoingCommand);
           
             continue;
          }
       }

       buffer -> data = command;
       buffer -> dataLength = outgoingCommand -> command.header.commandLength;
      
       host -> packetSize += buffer -> dataLength;

       * command = outgoingCommand -> command;
       
       enet_list_remove (& outgoingCommand -> outgoingCommandList);

       if (outgoingCommand -> packet != NULL)
       {
          ++ buffer;
          
          buffer -> data = outgoingCommand -> packet -> data;
          buffer -> dataLength = outgoingCommand -> packet -> dataLength;

          command -> header.commandLength += buffer -> dataLength;

          host -> packetSize += buffer -> dataLength;

          enet_list_insert (enet_list_end (& peer -> sentUnreliableCommands), outgoingCommand);
       }
       else
         enet_free (outgoingCommand);

       command -> header.commandLength = ENET_HOST_TO_NET_32 (command -> header.commandLength);

       ++ command;
       ++ buffer;
    } 

    host -> commandCount = command - host -> commands;
    host -> bufferCount = buffer - host -> buffers;
}

static int
enet_protocol_check_timeouts (ENetHost * host, ENetPeer * peer, ENetEvent * event)
{
    ENetOutgoingCommand * outgoingCommand;
    ENetListIterator currentCommand;

    currentCommand = enet_list_begin (& peer -> sentReliableCommands);

    while (currentCommand != enet_list_end (& peer -> sentReliableCommands))
    {
       outgoingCommand = (ENetOutgoingCommand *) currentCommand;

       currentCommand = enet_list_next (currentCommand);

       if (ENET_TIME_DIFFERENCE (timeCurrent, outgoingCommand -> sentTime) < outgoingCommand -> roundTripTimeout)
         continue;

       if (outgoingCommand -> roundTripTimeout >= outgoingCommand -> roundTripTimeoutLimit)
       {
          event -> type = ENET_EVENT_TYPE_DISCONNECT;
          event -> peer = peer;

          enet_peer_reset (peer);

          return 1;
       }

       if (outgoingCommand -> packet != NULL)
         peer -> reliableDataInTransit -= outgoingCommand -> fragmentLength;
          
       ++ peer -> packetsLost;

       outgoingCommand -> roundTripTimeout *= 2;

       enet_list_insert (enet_list_begin (& peer -> outgoingReliableCommands),
                         enet_list_remove (& outgoingCommand -> outgoingCommandList));

       if (currentCommand == enet_list_begin (& peer -> sentReliableCommands) &&
           enet_list_empty (& peer -> sentReliableCommands) == 0)
       {
          outgoingCommand = (ENetOutgoingCommand *) currentCommand;

          peer -> nextTimeout = outgoingCommand -> sentTime + outgoingCommand -> roundTripTimeout;
       }
    }
    
    return 0;
}

static void
enet_protocol_send_reliable_outgoing_commands (ENetHost * host, ENetPeer * peer)
{
    ENetProtocol * command = & host -> commands [host -> commandCount];
    ENetBuffer * buffer = & host -> buffers [host -> bufferCount];
    ENetOutgoingCommand * outgoingCommand;
    ENetListIterator currentCommand;

    currentCommand = enet_list_begin (& peer -> outgoingReliableCommands);
    
    while (currentCommand != enet_list_end (& peer -> outgoingReliableCommands))
    {
       outgoingCommand = (ENetOutgoingCommand *) currentCommand;

       if (command >= & host -> commands [sizeof (host -> commands) / sizeof (ENetProtocol)] ||
           peer -> packetSize - host -> packetSize < outgoingCommand -> command.header.commandLength)
         break;

       currentCommand = enet_list_next (currentCommand);

       if (outgoingCommand -> packet != NULL)
       {
          if ((uint16) (peer -> packetSize - host -> packetSize) <
                (uint16) (outgoingCommand -> command.header.commandLength +
                           outgoingCommand -> fragmentLength) ||
              peer -> reliableDataInTransit + outgoingCommand -> fragmentLength > peer -> windowSize)
            break;
       }
       else
       if (outgoingCommand -> command.header.command == ENET_PROTOCOL_COMMAND_CONNECT)
         peer -> challenge = (uint32) rand ();
       
       if (outgoingCommand -> roundTripTimeout == 0)
       {
          outgoingCommand -> roundTripTimeout = peer -> roundTripTime + 4 * peer -> roundTripTimeVariance;
          outgoingCommand -> roundTripTimeoutLimit = ENET_PEER_TIMEOUT_LIMIT * outgoingCommand -> roundTripTimeout;
       }

       if (enet_list_empty (& peer -> sentReliableCommands))
         peer -> nextTimeout = timeCurrent + outgoingCommand -> roundTripTimeout;

       enet_list_insert (enet_list_end (& peer -> sentReliableCommands),
                         enet_list_remove (& outgoingCommand -> outgoingCommandList));

       outgoingCommand -> sentTime = timeCurrent;

       buffer -> data = command;
       buffer -> dataLength = outgoingCommand -> command.header.commandLength;

       host -> packetSize += buffer -> dataLength;

       * command = outgoingCommand -> command;

       if (outgoingCommand -> packet != NULL)
       {
          ++ buffer;
          
          buffer -> data = outgoingCommand -> packet -> data + outgoingCommand -> fragmentOffset;
          buffer -> dataLength = outgoingCommand -> fragmentLength;

          command -> header.commandLength += outgoingCommand -> fragmentLength;

          host -> packetSize += outgoingCommand -> fragmentLength;

          peer -> reliableDataInTransit += outgoingCommand -> fragmentLength;
       }

       command -> header.commandLength = ENET_HOST_TO_NET_32 (command -> header.commandLength);

       ++ peer -> packetsSent;
        
       ++ command;
       ++ buffer;
    }

    host -> commandCount = command - host -> commands;
    host -> bufferCount = buffer - host -> buffers;
}

static int
enet_protocol_send_outgoing_commands (ENetHost * host, ENetEvent * event, int checkForTimeouts)
{
    size_t packetsSent = 1;
    ENetProtocolHeader header;
    ENetPeer * currentPeer;
    int sentLength;
    
    while (packetsSent > 0)
    for (currentPeer = host -> peers,
           packetsSent = 0;
         currentPeer < & host -> peers [host -> peerCount];
         ++ currentPeer)
    {
        if (currentPeer -> state == ENET_PEER_STATE_DISCONNECTED)
          continue;

        host -> commandCount = 0;
        host -> bufferCount = 1;
        host -> packetSize = sizeof (ENetProtocolHeader);

        if (enet_list_empty (& currentPeer -> acknowledgements) == 0)
          enet_protocol_send_acknowledgements (host, currentPeer);
     
        if (host -> commandCount < sizeof (host -> commands) / sizeof (ENetProtocol))
        {
           if (checkForTimeouts != 0 &&
               enet_list_empty (& currentPeer -> sentReliableCommands) == 0 &&
               ENET_TIME_GREATER_EQUAL (timeCurrent, currentPeer -> nextTimeout) &&
               enet_protocol_check_timeouts (host, currentPeer, event) == 1)
             return 1;
           
           if (enet_list_empty (& currentPeer -> outgoingReliableCommands) == 0)
             enet_protocol_send_reliable_outgoing_commands (host, currentPeer);
           else
           if (enet_list_empty (& currentPeer -> sentReliableCommands) &&
               ENET_TIME_DIFFERENCE (timeCurrent, currentPeer -> lastReceiveTime) >= ENET_PEER_PING_INTERVAL &&
               currentPeer -> packetSize - host -> packetSize >= sizeof (ENetProtocolPing))
           { 
              enet_peer_ping (currentPeer);
              enet_protocol_send_reliable_outgoing_commands (host, currentPeer);
           }
        }
                      
        if (host -> commandCount < sizeof (host -> commands) / sizeof (ENetProtocol) &&
            enet_list_empty (& currentPeer -> outgoingUnreliableCommands) == 0)
          enet_protocol_send_unreliable_outgoing_commands (host, currentPeer);

        if (host -> commandCount == 0)
          continue;

        if (currentPeer -> packetLossEpoch == 0)
          currentPeer -> packetLossEpoch = timeCurrent;
        else
        if (ENET_TIME_DIFFERENCE (timeCurrent, currentPeer -> packetLossEpoch) >= ENET_PEER_PACKET_LOSS_INTERVAL &&
            currentPeer -> packetsSent > 0)
        {
           uint32 packetLoss = currentPeer -> packetsLost * ENET_PEER_PACKET_LOSS_SCALE / currentPeer -> packetsSent;

#ifdef ENET_DEBUG
           fprintf (stderr, "peer %u: %f%%+-%f%% packet loss, %u+-%u ms round trip time, %f%% throttle, %u/%u outgoing, %u/%u incoming\n", currentPeer -> incomingPeerID, currentPeer -> packetLoss / (float) ENET_PEER_PACKET_LOSS_SCALE, currentPeer -> packetLossVariance / (float) ENET_PEER_PACKET_LOSS_SCALE, currentPeer -> roundTripTime, currentPeer -> roundTripTimeVariance, currentPeer -> packetThrottle / (float) ENET_PEER_PACKET_THROTTLE_SCALE, enet_list_size (& currentPeer -> outgoingReliableCommands), enet_list_size (& currentPeer -> outgoingUnreliableCommands), enet_list_size (& currentPeer -> channels -> incomingReliableCommands), enet_list_size (& currentPeer -> channels -> incomingUnreliableCommands));
#endif
          
           currentPeer -> packetLossVariance -= currentPeer -> packetLossVariance / 4;

           if (packetLoss >= currentPeer -> packetLoss)
           {
              currentPeer -> packetLoss += (packetLoss - currentPeer -> packetLoss) / 8;
              currentPeer -> packetLossVariance += (packetLoss - currentPeer -> packetLoss) / 4;
           }
           else
           {
              currentPeer -> packetLoss -= (currentPeer -> packetLoss - packetLoss) / 8;
              currentPeer -> packetLossVariance += (currentPeer -> packetLoss - packetLoss) / 4;
           }

           currentPeer -> packetLossEpoch = timeCurrent;
           currentPeer -> packetsSent = 0;
           currentPeer -> packetsLost = 0;
        }

        header.peerID = ENET_HOST_TO_NET_16 (currentPeer -> outgoingPeerID);
        header.flags = 0;
        header.commandCount = host -> commandCount;
        header.sentTime = ENET_HOST_TO_NET_32 (timeCurrent);
        header.challenge = currentPeer -> challenge;

        host -> buffers -> data = & header;
        host -> buffers -> dataLength = sizeof (ENetProtocolHeader);

        currentPeer -> lastSendTime = timeCurrent;

        ++ packetsSent;

        sentLength = enet_socket_send (host -> socket, & currentPeer -> address, host -> buffers, host -> bufferCount);

        enet_protocol_remove_sent_unreliable_commands (currentPeer);

        if (sentLength < 0)
          return -1;
    }
   
    return 0;
}

void
enet_host_flush (ENetHost * host)
{
    timeCurrent = enet_time_get ();

    enet_protocol_send_outgoing_commands (host, NULL, 0);
}

int
enet_host_service (ENetHost * host, ENetEvent * event, uint32 timeout)
{
    uint32 waitCondition;

    event -> type = ENET_EVENT_TYPE_NONE;
    event -> peer = NULL;
    event -> packet = NULL;

    switch (enet_protocol_dispatch_incoming_commands (host, event))
    {
    case 1:
       return 1;

    case -1:
       perror ("Error dispatching incoming packets");

       return -1;

    default:
       break;
    }
   
    timeCurrent = enet_time_get ();
    
    timeout += timeCurrent;

    do
    {
       if (ENET_TIME_DIFFERENCE (timeCurrent, host -> bandwidthThrottleEpoch) >= ENET_HOST_BANDWIDTH_THROTTLE_INTERVAL)
         enet_host_bandwidth_throttle (host);

       switch (enet_protocol_receive_incoming_commands (host, event))
       {
       case 1:
          return 1;

       case -1:
          perror ("Error receiving incoming packets");

          return -1;

       default:
          break;
       }

       switch (enet_protocol_send_outgoing_commands (host, event, 1))
       {
       case 1:
          return 1;

       case -1:
          perror ("Error sending outgoing packets");

          return -1;

       default:
          break;
       }

       switch (enet_protocol_dispatch_incoming_commands (host, event))
       {
       case 1:
          return 1;

       case -1:
          perror ("Error dispatching incoming packets");

          return -1;

       default:
          break;
       }

       timeCurrent = enet_time_get ();

       if (ENET_TIME_GREATER_EQUAL (timeCurrent, timeout))
         return 0;

       waitCondition = ENET_SOCKET_WAIT_RECEIVE;

       if (enet_socket_wait (host -> socket, & waitCondition, ENET_TIME_DIFFERENCE (timeout, timeCurrent)) != 0)
         return -1;
       
       timeCurrent = enet_time_get ();
    } while (waitCondition == ENET_SOCKET_WAIT_RECEIVE);

    return 0; 
}

