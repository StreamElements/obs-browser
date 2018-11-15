#include "NamedPipesServer.hpp"

#include <algorithm>

static const size_t BUFFER_SIZE = 512;
static const int CLIENT_TIMEOUT_MS = 5000;

NamedPipesServer::NamedPipesServer(
	const char* const pipeName,
	NamedPipesServerClientHandler::msg_handler_t msgHandler,
	size_t maxClients) :
	m_pipeName(pipeName),
	m_msgHandler(msgHandler),
	m_maxClients(maxClients),
	m_running(false)
{
}

NamedPipesServer::~NamedPipesServer()
{
	Stop();
}

void NamedPipesServer::Start()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_running) {
		return;
	}

	m_running = true;

	m_thread = std::thread([this]() {
		ThreadProc();
	});
}

void NamedPipesServer::Stop()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_running) {
		return;
	}

	m_running = false;

	if (m_thread.joinable()) {
		m_thread.join();
	}
}

bool NamedPipesServer::IsRunning()
{
	return m_running;
}

bool NamedPipesServer::CanAddHandler()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return (m_clients.size() < m_maxClients);
}

void NamedPipesServer::RemoveDisconnectedClients()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	std::vector<NamedPipesServerClientHandler*> removeClients;

	for (auto client : m_clients) {
		if (!client->IsConnected()) {
			removeClients.push_back(client);
		}
	}

	for (auto client : removeClients) {
		m_clients.remove(client);

		delete client;
	}
}

void NamedPipesServer::DisconnectAllClients()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	for (auto client : m_clients) {
		client->Disconnect();
	}
}

void NamedPipesServer::SendMessage(const char* const buffer, const size_t length)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	for (auto client : m_clients) {
		client->SendMessage(buffer, length);
	}

	RemoveDisconnectedClients();
}

void NamedPipesServer::ThreadProc()
{
	while (m_running || !m_clients.empty()) {
		RemoveDisconnectedClients();

		if (!m_running) {
			DisconnectAllClients();
			RemoveDisconnectedClients();

			break;
		}

		if (!CanAddHandler()) {
			Sleep(CLIENT_TIMEOUT_MS);
		}
		else {
			HANDLE hPipe = ::CreateNamedPipeA(
				m_pipeName.c_str(),
				PIPE_ACCESS_DUPLEX,
				PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
				m_maxClients,
				BUFFER_SIZE,
				BUFFER_SIZE,
				CLIENT_TIMEOUT_MS,
				NULL);

			if (hPipe == INVALID_HANDLE_VALUE) {
				printf("CreateNamedPipe failed: %d\n", GetLastError());

				Sleep(CLIENT_TIMEOUT_MS);
			}

			const bool isConnected =
				ConnectNamedPipe(hPipe, NULL) ?
				TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

			if (isConnected) {
				printf("ConnectNamedPipe: client connected\n");

				m_clients.push_back(new NamedPipesServerClientHandler(hPipe, m_msgHandler));
			}
			else {
				CloseHandle(hPipe);
			}
		}
	}

	m_running = false;
}
