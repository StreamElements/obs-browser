#pragma once

#include <windows.h>
#include <string>
#include <thread>
#include <mutex>

class NamedPipesServerClientHandler
{
public:
	typedef std::function<void(const char* const, const size_t)> msg_handler_t;

public:
	NamedPipesServerClientHandler(HANDLE hPipe, msg_handler_t msgHandler);
	virtual ~NamedPipesServerClientHandler();

public:
	bool IsConnected();
	void Disconnect();
	bool SendMessage(const char* const buffer, size_t length);

private:
	void ThreadProc();

private:
	HANDLE m_hPipe;
	std::thread m_thread;
	msg_handler_t m_msgHandler;
	std::recursive_mutex m_mutex;
};

