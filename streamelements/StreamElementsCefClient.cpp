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

void StreamElementsCefClient::OnLoadEnd(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	int httpStatusCode)
{
	if (m_executeJavaScriptCodeOnLoad.empty() || !frame->IsMain()) {
		return;
	}

	frame->ExecuteJavaScript(
		CefString(m_executeJavaScriptCodeOnLoad),
		frame->GetURL(),
		0);
}

/* ========================================================================= */

bool StreamElementsCefClient::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
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
	else if (m_messageHandler.get() && m_messageHandler->OnProcessMessageReceived(browser, source_process, message)) {
		return true;
	}
	else {

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
