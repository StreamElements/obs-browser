#pragma once

#include <mutex>
#include <list>
#include <map>

#include "cef-headers.hpp"

#include "StreamElementsControllerServer.hpp"

class StreamElementsMessageBus
{
public:
	typedef uint32_t message_destination_filter_flags_t;

	static const message_destination_filter_flags_t DEST_ALL;

	static const message_destination_filter_flags_t DEST_ALL_LOCAL;
	static const message_destination_filter_flags_t DEST_UI;
	static const message_destination_filter_flags_t DEST_WORKER;
	static const message_destination_filter_flags_t DEST_BROWSER_SOURCE;

	static const message_destination_filter_flags_t DEST_ALL_EXTERNAL;
	static const message_destination_filter_flags_t DEST_EXTERNAL_CONTROLLER;

	static const char* const SOURCE_APPLICATION;
	static const char* const SOURCE_WEB;
	static const char* const SOURCE_EXTERNAL;

protected:
	StreamElementsMessageBus();
	virtual ~StreamElementsMessageBus();

public:
	static StreamElementsMessageBus* GetInstance();

public:
	void AddBrowserListener(CefRefPtr<CefBrowser> browser, message_destination_filter_flags_t type);
	void RemoveBrowserListener(CefRefPtr<CefBrowser> browser);

public:
	void NotifyAllLocalEventListeners(
		message_destination_filter_flags_t types,
		std::string source,
		std::string sourceAddress,
		std::string event,
		CefRefPtr<CefValue> payload);

	void NotifyAllExternalEventListeners(
		message_destination_filter_flags_t types,
		std::string source,
		std::string sourceAddress,
		std::string event,
		CefRefPtr<CefValue> payload);

	void NotifyAllMessageListeners(
		message_destination_filter_flags_t types,
		std::string source,
		std::string sourceAddress,
		CefRefPtr<CefValue> payload)
	{
		HandleSystemCommands(types, source, sourceAddress, payload);

		if (DEST_ALL_LOCAL & types) {
			NotifyAllLocalEventListeners(
				types,
				source,
				sourceAddress,
				"hostMessageReceived",
				payload);
		}

		if (DEST_ALL_EXTERNAL & types) {
			m_controller_server.SendMessageAllClients(
				source,
				sourceAddress,
				payload);
		}
	}

protected:
	virtual void HandleSystemCommands(
		message_destination_filter_flags_t types,
		std::string source,
		std::string sourceAddress,
		CefRefPtr<CefValue> payload);

public:
	virtual void PublishSystemState();

private:
	std::recursive_mutex m_browser_list_mutex;
	std::map<CefRefPtr<CefBrowser>, message_destination_filter_flags_t> m_browser_list;
	StreamElementsControllerServer m_controller_server;

private:
	static StreamElementsMessageBus* s_instance;
};
