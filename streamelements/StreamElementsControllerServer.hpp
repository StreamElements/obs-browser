#pragma once

#include "deps/server/NamedPipesServer.hpp"
#include "cef-headers.hpp"

class StreamElementsMessageBus;

class StreamElementsControllerServer
{
public:
	StreamElementsControllerServer(StreamElementsMessageBus* bus);
	virtual ~StreamElementsControllerServer();

public:
	void NotifyAllClients(
		std::string source,
		std::string sourceAddress,
		CefRefPtr<CefValue> payload);

	void SendEventAllClients(
		std::string source,
		std::string sourceAddress,
		std::string eventName,
		CefRefPtr<CefValue> eventData);

	void SendMessageAllClients(
		std::string source,
		std::string sourceAddress,
		CefRefPtr<CefValue> message);

private:
	void OnMsgReceivedInternal(std::string& msg);

private:
	StreamElementsMessageBus* m_bus = nullptr;
	NamedPipesServer* m_server = nullptr;
};
