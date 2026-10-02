#pragma once

#include <queue>
#include <set>
#include <map>
#include "Socket.h"

uint32 THREADCALL SocketCleanupThread(void * lpParam);

class SocketMgr
{
public:
	SocketMgr();

	void Initialise();

	void SpawnWorkerThreads();
	void ShutdownThreads();

	static void SetupSockets();
	static void CleanupSockets();

	INLINE HANDLE GetCompletionPort() { return m_completionPort; }
	INLINE void SetCompletionPort(HANDLE cp) { m_completionPort = cp; }
	void CreateCompletionPort();

	static void SetupWinsock();
	static void CleanupWinsock();
	
	static uint32 THREADCALL SocketWorkerThread(void * lpParam);

	HANDLE m_completionPort;

	virtual Socket *AssignSocket(SOCKET socket) = 0;
	virtual void OnConnect(Socket *pSock);
	virtual void OnDisconnect(Socket *pSock);
	virtual void DisconnectCallback(Socket *pSock);
	virtual void Shutdown();
	virtual ~SocketMgr();

	static std::recursive_mutex s_disconnectionQueueLock;
	static std::queue<Socket *> s_disconnectionQueue;

protected:
	bool m_bShutdown;

	Thread * m_thread;
	static Thread s_cleanupThread;

	long m_threadCount;
	bool m_bWorkerThreadsActive;

	INLINE void IncRef() { if (s_refCounter.increment() == 1) SetupSockets(); }
	INLINE void DecRef() { if (s_refCounter.decrement() == 0) CleanupSockets(); }

	// reference counter (one app can hold multiple socket manager instances)
	static Atomic<uint32> s_refCounter;

public:
	// Bot tick: a timer thread posts SOCKET_IO_EVENT_BOT_TICK; the IOCP worker runs the handler.
	typedef void (*BotTickHandler)();
	static void SetBotTickHandler(BotTickHandler handler);
	// Posts one BOT_TICK event. Returns false (and posts nothing) if the previous one has not
	// been consumed yet or the post failed, so the queue never accumulates ticks.
	bool PostBotTick();

	static BotTickHandler s_botTickHandler;
	static std::atomic<bool> s_botTickPending;

	static bool s_bRunningCleanupThread;
};

typedef void(*OperationHandler)(Socket * s, uint32 len);

void HandleReadComplete(Socket * s, uint32 len);
void HandleWriteComplete(Socket * s, uint32 len);
void HandleShutdown(Socket * s, uint32 len);
void HandleBotTick(Socket * s, uint32 len);

static OperationHandler ophandlers[] =
{
	&HandleReadComplete,
	&HandleWriteComplete,
	&HandleShutdown,
	&HandleBotTick
};
