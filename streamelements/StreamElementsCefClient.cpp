#include "StreamElementsCefClient.hpp"
#include "base64/base64.hpp"
#include "json11/json11.hpp"
#include <obs-frontend-api.h>

using namespace json11;

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
