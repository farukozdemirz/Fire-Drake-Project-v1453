#pragma once

class Packet;

// Receiver for the packets a bot session would otherwise send to a client socket.
// Real (socket) sessions never have a sink.
class IBotSink
{
public:
	virtual ~IBotSink() {}
	virtual void OnPacket(Packet & pkt) = 0;
};
