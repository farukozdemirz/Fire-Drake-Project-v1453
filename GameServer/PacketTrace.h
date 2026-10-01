#pragma once

#ifdef FDP_PACKET_TRACE

class Packet;

namespace PacketTrace
{
	// Writes an incoming packet from an in-game player to the trace file.
	// Called from CUser::HandlePacket, which runs on an IOCP worker thread.
	void LogIncoming(uint16 sid, const char * charName, uint8 zone, const Packet & pkt);
}

#endif
