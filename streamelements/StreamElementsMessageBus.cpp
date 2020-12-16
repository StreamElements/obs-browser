#include "StreamElementsMessageBus.hpp"
#include "StreamElementsConfig.hpp"
#include "StreamElementsGlobalStateManager.hpp"

StreamElementsMessageBus* StreamElementsMessageBus::s_instance = nullptr;

const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_ALL = 0xFFFFFFFFUL;

const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_ALL_LOCAL = 0x0000FFFFUL;
const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_UI = 0x00000001UL;
const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_WORKER = 0x00000002UL;
const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_BROWSER_SOURCE = 0x00000004UL;

const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_ALL_EXTERNAL = 0xFFFF0000UL;
const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_EXTERNAL_CONTROLLER = 0x00010000UL;

const char* const StreamElementsMessageBus::SOURCE_APPLICATION = "application";
const char* const StreamElementsMessageBus::SOURCE_WEB = "web";
const char* const StreamElementsMessageBus::SOURCE_EXTERNAL = "external";

StreamElementsMessageBus::StreamElementsMessageBus() :
	m_external_controller_server(this)
{

}

StreamElementsMessageBus::~StreamElementsMessageBus()
{

}

StreamElementsMessageBus* StreamElementsMessageBus::GetInstance()
{
	if (!s_instance) {
		s_instance = new StreamElementsMessageBus();
	}

	return s_instance;
}

void StreamElementsMessageBus::AddBrowserListener(CefRefPtr<CefBrowser> browser, message_destination_filter_flags_t type)
{
	std::lock_guard<std::recursive_mutex> guard(m_browser_list_mutex);

	m_browser_list.emplace(browser, type);

	auto httpRequestHandler = [this, browser](const HttpServer::request_t &req,
					 HttpServer::response_t &res) -> void {

		CefRefPtr<CefValue> root = CefValue::Create();

		CefRefPtr<CefDictionaryValue> rootDict = CefDictionaryValue::Create();

		// Method, path
		rootDict->SetString("method", req.method.c_str());
		rootDict->SetString("url", req.path.c_str());

		// Body
		rootDict->SetString("body", req.body.c_str());

		// Headers
		CefRefPtr<CefDictionaryValue> headers =
			CefDictionaryValue::Create();

		for (auto kv : req.headers) {
			headers->SetString(kv.first, kv.second);
		}

		rootDict->SetDictionary("headers", headers);

		// Query string params
		CefRefPtr<CefDictionaryValue> query =
			CefDictionaryValue::Create();

		for (auto kv : req.params) {
			query->SetString(kv.first, kv.second);
		}

		rootDict->SetDictionary("query", query);

		root->SetDictionary(rootDict);

		NotifyBrowserEventListener(browser, "browser", "http",
					   "urn:http:server:browser",
					   "hostMessageReceived", root);

		res.set_content("{ \"success\": true }", "application/json");
	};

	m_browser_http_servers[browser->GetIdentifier()] =
		std::make_shared<StreamElementsHttpServerManager>(
			httpRequestHandler);
}

void StreamElementsMessageBus::RemoveBrowserListener(CefRefPtr<CefBrowser> browser)
{
	std::lock_guard<std::recursive_mutex> guard(m_browser_list_mutex);

	m_browser_list.erase(browser);

	m_browser_http_servers.erase(browser->GetIdentifier());
}

void StreamElementsMessageBus::DeserializeBrowserHttpServer(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefValue> input,
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<std::recursive_mutex> guard(m_browser_list_mutex);

	output->SetNull();

	if (!m_browser_http_servers.count(browser->GetIdentifier()))
		return;

	m_browser_http_servers[browser->GetIdentifier()]->DeserializeHttpServer(
		input, output);
}

void StreamElementsMessageBus::SerializeBrowserHttpServers(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefValue> &output)
{
	std::lock_guard<std::recursive_mutex> guard(m_browser_list_mutex);

	output->SetNull();

	if (!m_browser_http_servers.count(browser->GetIdentifier()))
		return;

	m_browser_http_servers[browser->GetIdentifier()]->SerializeHttpServers(
		output);
}

void StreamElementsMessageBus::RemoveBrowserHttpServersByIds(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefValue> input,
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<std::recursive_mutex> guard(m_browser_list_mutex);

	output->SetNull();

	if (!m_browser_http_servers.count(browser->GetIdentifier()))
		return;

	m_browser_http_servers[browser->GetIdentifier()]->RemoveHttpServersByIds(
		input, output);
}

void StreamElementsMessageBus::NotifyBrowserEventListener(
	CefRefPtr<CefBrowser> browser, std::string scope, std::string source,
	std::string sourceAddress, std::string event,
	CefRefPtr<CefValue> payload)
{
	CefRefPtr<CefValue> root = CefValue::Create();
	CefRefPtr<CefDictionaryValue> rootDict = CefDictionaryValue::Create();

	rootDict->SetString("scope", scope);
	rootDict->SetString("source", source);
	rootDict->SetString("sourceAddress", sourceAddress);
	rootDict->SetValue("message", payload->Copy());

	root->SetDictionary(rootDict);

	CefString payloadJson = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create("DispatchJSEvent");
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetString(0, event);
	args->SetString(1, payloadJson);

	SendBrowserProcessMessage(browser, PID_RENDERER, msg);
}

void StreamElementsMessageBus::NotifyAllLocalEventListeners(
	message_destination_filter_flags_t types,
	std::string source,
	std::string sourceAddress,
	std::string event,
	CefRefPtr<CefValue> payload)
{
	std::lock_guard<std::recursive_mutex> guard(m_browser_list_mutex);

	if (m_browser_list.empty()) {
		return;
	}

	CefRefPtr<CefValue> root = CefValue::Create();
	CefRefPtr<CefDictionaryValue> rootDict = CefDictionaryValue::Create();

	rootDict->SetString("scope", "broadcast");
	rootDict->SetString("source", source);
	rootDict->SetString("sourceAddress", sourceAddress);
	rootDict->SetValue("message", payload->Copy());

	root->SetDictionary(rootDict);

	CefString payloadJson = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	for (auto kv : m_browser_list) {
		auto browser = kv.first;

		if (kv.second & types) {
			CefRefPtr<CefProcessMessage> msg =
				CefProcessMessage::Create("DispatchJSEvent");
			CefRefPtr<CefListValue> args = msg->GetArgumentList();

			args->SetString(0, event);
			args->SetString(1, payloadJson);

			SendBrowserProcessMessage(browser, PID_RENDERER, msg);
		}
	}
}

void StreamElementsMessageBus::NotifyAllExternalEventListeners(
	message_destination_filter_flags_t types,
	std::string source,
	std::string sourceAddress,
	std::string event,
	CefRefPtr<CefValue> payload)
{
	if (DEST_EXTERNAL_CONTROLLER & types) {
		m_external_controller_server.SendEventAllClients(
			source,
			sourceAddress,
			event,
			payload);
	}
}

bool StreamElementsMessageBus::HandleSystemCommands(
	message_destination_filter_flags_t types,
	std::string source,
	std::string sourceAddress,
	CefRefPtr<CefValue> payload)
{
	if (source != SOURCE_EXTERNAL || payload->GetType() != VTYPE_DICTIONARY) {
		return false;
	}

	CefRefPtr<CefDictionaryValue> root = payload->GetDictionary();

	if (!root->HasKey("payload") || root->GetType("payload") != VTYPE_DICTIONARY) {
		return false;
	}

	CefRefPtr<CefDictionaryValue> payloadDict = root->GetDictionary("payload");

	if (!payloadDict->HasKey("class") ||
		!payloadDict->HasKey("command") ||
		payloadDict->GetType("command") != VTYPE_DICTIONARY ||
		payloadDict->GetString("class") != "command") {
		return false;
	}

	CefRefPtr<CefDictionaryValue> commandDict = payloadDict->GetDictionary("command");

	if (!commandDict->HasKey("name") || commandDict->GetType("name") != VTYPE_STRING) {
		return false;
	}

	std::string commandId = commandDict->GetString("name");

	if (commandId == "SYS$QUERY:STATE") {
		PublishSystemState();

		return true;
	}

	return false;
}

void StreamElementsMessageBus::PublishSystemState()
{
	CefRefPtr<CefValue> root = CefValue::Create();
	CefRefPtr<CefDictionaryValue> payload = CefDictionaryValue::Create();

	bool isLoggedIn =
		StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN == (StreamElementsConfig::GetInstance()->GetStartupFlags() & StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN);

	payload->SetBool("isSignedIn", isLoggedIn);

	root->SetDictionary(payload);

	NotifyAllExternalEventListeners(
		DEST_ALL_EXTERNAL,
		SOURCE_APPLICATION,
		"OBS",
		"SYS$REPORT:STATE",
		root);
}
