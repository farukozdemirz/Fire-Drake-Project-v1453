#pragma once

#ifdef FDP_PACKET_TRACE

class Packet;

namespace PacketTrace
{
	// Writes an incoming packet from an in-game player to the trace file.
	// Called from CUser::HandlePacket, which runs on an IOCP worker thread.
	void LogIncoming(uint16 sid, const char * charName, uint8 zone, const Packet & pkt);

	// Writes a packet the server sends to an in-game player (or bot) to the
	// trace file. Called from CUser::Send / CUser::SendCompressed, which run
	// on arbitrary threads.
	void LogOutgoing(uint16 sid, const char * charName, uint8 zone, const Packet & pkt);
}

#endif
