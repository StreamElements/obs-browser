#include "StreamElementsCefClient.hpp"
#include "base64/base64.hpp"
#include "json11/json11.hpp"
#include <obs-frontend-api.h>
#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

using namespace json11;

/* ========================================================================= */

#define CEF_REQUIRE_UI_THREAD()       DCHECK(CefCurrentlyOn(TID_UI));
#define CEF_REQUIRE_IO_THREAD()       DCHECK(CefCurrentlyOn(TID_IO));
#define CEF_REQUIRE_FILE_THREAD()     DCHECK(CefCurrentlyOn(TID_FILE));
#define CEF_REQUIRE_RENDERER_THREAD() DCHECK(CefCurrentlyOn(TID_RENDERER));

/* ========================================================================= */

bool StreamElementsCefClient::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
	CefProcessId,
	CefRefPtr<CefProcessMessage> message)
{
	const std::string &name = message->GetName();
	Json json;

	if (name == "getCurrentScene") {
		json = Json::object{};
	}
	else if (name == "getStatus") {
		json = Json::object{
			{ "recording", obs_frontend_recording_active() },
			{ "streaming", obs_frontend_streaming_active() },
			{ "replaybuffer", obs_frontend_replay_buffer_active() }
		};

	}
	else if (name == "CefRenderProcessHandler::OnContextCreated") {
		// Context created, request creation of window.host object
		// with API methods
		CefRefPtr<CefValue> root = CefValue::Create();

		CefRefPtr<CefDictionaryValue> rootDictionary = CefDictionaryValue::Create();
		root->SetDictionary(rootDictionary);

		auto addFunction = [&](
			const char* functionName,
			const char* messageName,
			const int numInputArgs)
		{
			CefRefPtr<CefDictionaryValue> function = CefDictionaryValue::Create();

			function->SetString("message", messageName);
			function->SetInt("numInputArgs", numInputArgs);

			rootDictionary->SetDictionary(functionName, function);
		};

		// Add function definitions
		addFunction("getWidgets", "StreamElementsCefClient::GetWidgets", 0);
		addFunction("addWidget", "StreamElementsCefClient::AddWidget", 0);
		addFunction("removeAllWidgets", "StreamElementsCefClient::RemoveAllWidgets", 0);
		addFunction("removeWidgetById", "StreamElementsCefClient::RemoveWidgetById", 0);

		// Convert data to JSON
		CefString jsonString =
			CefWriteJSON(root, JSON_WRITER_DEFAULT);

		// Send request to renderer process
		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("CefRenderProcessHandler::BindJavaScriptFunctions");
		msg->GetArgumentList()->SetString(0, "host");
		msg->GetArgumentList()->SetString(1, jsonString);
		browser->SendProcessMessage(PID_RENDERER, msg);

		return true;
	}
	else {
		// ::MessageBoxA(0, name.c_str(), name.c_str(), 0);

		return false;
	}

	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create("executeCallback");

	CefRefPtr<CefListValue> args = msg->GetArgumentList();
	args->SetInt(0, message->GetArgumentList()->GetInt(0));
	args->SetString(1, json.dump());

	browser->SendProcessMessage(PID_RENDERER, msg);

	return true;
}
