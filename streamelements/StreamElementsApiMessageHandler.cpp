#include "StreamElementsApiMessageHandler.hpp"

#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

#include "StreamElementsConfig.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsUtils.hpp"

#include <QDesktopServices>
#include <QUrl>

/* Numeric value indicating the current major version of the API.
 * This value must be incremented each time a breaking change to
 * the API is introduced(change of existing API methods/properties
 * signatures).
 */
const int API_VERSION_MAJOR = 1;

/* Numeric value indicating the current minor version of the API.
 * This value will be incremented each time a non-breaking change
 * to the API is introduced (additional functionality, bugfixes
 * of existing functionality).
 */
const int API_VERSION_MINOR = 0;

/* Incoming messages from renderer process */
const char* MSG_ON_CONTEXT_CREATED = "CefRenderProcessHandler::OnContextCreated";
const char* MSG_INCOMING_API_CALL = "StreamElementsApiMessageHandler::OnIncomingApiCall";

/* Outgoing messages to renderer process */
const char* MSG_BIND_JAVASCRIPT_FUNCTIONS = "CefRenderProcessHandler::BindJavaScriptFunctions";
const char* MSG_BIND_JAVASCRIPT_PROPS = "CefRenderProcessHandler::BindJavaScriptProperties";

bool StreamElementsApiMessageHandler::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message)
{
	const std::string &name = message->GetName();

	if (name == MSG_ON_CONTEXT_CREATED) {
		RegisterIncomingApiCallHandlersInternal(browser);
		RegisterApiPropsInternal(browser);
		DispatchHostReadyEventInternal(browser);

		return true;
	}
	else if (name == MSG_INCOMING_API_CALL) {
		CefRefPtr<CefValue> result = CefValue::Create();
		result->SetBool(false);

		CefRefPtr<CefListValue> args = message->GetArgumentList();

		const int headerSize = args->GetInt(0);
		std::string id = args->GetString(2).ToString();

		id = id.substr(id.find_last_of('.') + 1); // window.host.XXX -> XXX

		if (m_apiCallHandlers.count(id)) {
			CefRefPtr<CefListValue> callArgs = CefListValue::Create();

			for (int i = headerSize; i < args->GetSize() - 1; ++i) {
				CefRefPtr<CefValue> parsedValue =
					CefParseJSON(
						args->GetString(i),
						JSON_PARSER_ALLOW_TRAILING_COMMAS);

				callArgs->SetValue(
					callArgs->GetSize(),
					parsedValue);
			}

			struct local_context {
				os_event_t* event;
				StreamElementsApiMessageHandler* self;
				std::string id;
				CefRefPtr<CefProcessMessage> message;
				CefRefPtr<CefListValue> callArgs;
				CefRefPtr<CefValue> result;
			};

			local_context* context = new local_context();

			os_event_init(&context->event, OS_EVENT_TYPE_AUTO);
			context->self = this;
			context->id = id;
			context->message = message;
			context->callArgs = callArgs;
			context->result = result;

			QtPostTask([](void* data)
			{
				local_context* context = (local_context*)data;

				context->self->m_apiCallHandlers[context->id](
					context->self,
					context->message,
					context->callArgs,
					context->result);

				os_event_signal(context->event);
			}, context);

			os_event_wait(context->event);
			os_event_destroy(context->event);

			delete context;
		}

		// Invoke result callback
		CefRefPtr<CefProcessMessage> msg =
			CefProcessMessage::Create("executeCallback");

		CefRefPtr<CefListValue> callbackArgs = msg->GetArgumentList();
		callbackArgs->SetInt(0, message->GetArgumentList()->GetInt(message->GetArgumentList()->GetSize() - 1));
		callbackArgs->SetString(1, CefWriteJSON(result, JSON_WRITER_DEFAULT));

		browser->SendProcessMessage(PID_RENDERER, msg);

		return true;
	}

	return false;
}

void StreamElementsApiMessageHandler::RegisterIncomingApiCallHandlersInternal(CefRefPtr<CefBrowser> browser)
{
	RegisterIncomingApiCallHandlers();

	// Context created, request creation of window.host object
	// with API methods
	CefRefPtr<CefValue> root = CefValue::Create();

	CefRefPtr<CefDictionaryValue> rootDictionary = CefDictionaryValue::Create();
	root->SetDictionary(rootDictionary);

	for (auto apiCallHandler : m_apiCallHandlers) {
		CefRefPtr<CefValue> val = CefValue::Create();

		CefRefPtr<CefDictionaryValue> function = CefDictionaryValue::Create();

		function->SetString("message", MSG_INCOMING_API_CALL);

		val->SetDictionary(function);

		rootDictionary->SetValue(apiCallHandler.first, val);
	}

	// Convert data to JSON
	CefString jsonString =
		CefWriteJSON(root, JSON_WRITER_DEFAULT);

	// Send request to renderer process
	CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create(MSG_BIND_JAVASCRIPT_FUNCTIONS);
	msg->GetArgumentList()->SetString(0, "host");
	msg->GetArgumentList()->SetString(1, jsonString);
	browser->SendProcessMessage(PID_RENDERER, msg);
}

void StreamElementsApiMessageHandler::RegisterApiPropsInternal(CefRefPtr<CefBrowser> browser)
{
	// Context created, request creation of window.host object
	// with API methods
	CefRefPtr<CefValue> root = CefValue::Create();

	CefRefPtr<CefDictionaryValue> rootDictionary = CefDictionaryValue::Create();
	root->SetDictionary(rootDictionary);

	rootDictionary->SetBool("hostReady", true);
	rootDictionary->SetInt("apiMajorVersion", API_VERSION_MAJOR);
	rootDictionary->SetInt("apiMinorVersion", API_VERSION_MINOR);

	// Convert data to JSON
	CefString jsonString =
		CefWriteJSON(root, JSON_WRITER_DEFAULT);

	// Send request to renderer process
	CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create(MSG_BIND_JAVASCRIPT_PROPS);
	msg->GetArgumentList()->SetString(0, "host");
	msg->GetArgumentList()->SetString(1, jsonString);
	browser->SendProcessMessage(PID_RENDERER, msg);
}

void StreamElementsApiMessageHandler::DispatchHostReadyEventInternal(CefRefPtr<CefBrowser> browser)
{
	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create("DispatchJSEvent");
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetString(0, "hostReady");
	args->SetString(1, "null");
	browser->SendProcessMessage(PID_RENDERER, msg);
}

void StreamElementsApiMessageHandler::RegisterIncomingApiCallHandler(std::string id, incoming_call_handler_t handler)
{
	m_apiCallHandlers[id] = handler;
}

#define API_HANDLER_BEGIN(name) RegisterIncomingApiCallHandler(name, [](StreamElementsApiMessageHandler*, CefRefPtr<CefProcessMessage> message, CefRefPtr<CefListValue> args, CefRefPtr<CefValue>& result) {
#define API_HANDLER_END() });

void StreamElementsApiMessageHandler::RegisterIncomingApiCallHandlers()
{
	//RegisterIncomingApiCallHandler("getWidgets", [](StreamElementsApiMessageHandler*, CefRefPtr<CefProcessMessage> message, CefRefPtr<CefListValue> args, CefRefPtr<CefValue>& result) {
	//	result->SetBool(true);
	//});

	API_HANDLER_BEGIN("getStartupFlags");
	{
		result->SetInt(StreamElementsConfig::GetInstance()->GetStartupFlags());
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setStartupFlags");
	{
		StreamElementsConfig::GetInstance()->SetStartupFlags(args->GetValue(0)->GetInt());

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("deleteAllCookies");
	{
		CefCookieManager::GetGlobalManager(NULL)->DeleteCookies(
			CefString(""), // URL
			CefString(""), // Cookie name
			nullptr);      // On-complete callback

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("openDefaultBrowser");
	{
		CefString url = args->GetValue(0)->GetString();

		QUrl navigate_url = QUrl(url.ToString().c_str(), QUrl::TolerantMode);
		QDesktopServices::openUrl(navigate_url);

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showNotificationBar");
	{
		CefRefPtr<CefValue> barInfo = args->GetValue(0);

		StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->DeserializeNotificationBar(barInfo);

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("hideNotificationBar");
	{
		StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->HideNotificationBar();

		result->SetBool(true);
	}
	API_HANDLER_END();
}
