#include "StreamElementsBandwidthTestManager.hpp"

extern void DispatchJSEvent(const char *eventName, const char *jsonString);

StreamElementsBandwidthTestManager::StreamElementsBandwidthTestManager()
{
	m_isTestInProgress = false;
	m_client = new StreamElementsBandwidthTestClient();
}

StreamElementsBandwidthTestManager::~StreamElementsBandwidthTestManager()
{
	delete m_client;
}

bool StreamElementsBandwidthTestManager::BeginBandwidthTest(CefRefPtr<CefValue> settingsValue, CefRefPtr<CefValue> serversValue)
{
	std::lock_guard<std::mutex> guard(m_mutex);

	if (m_isTestInProgress) {
		return false;
	}

	m_last_test_results.clear();

	CefRefPtr<CefDictionaryValue> settings = settingsValue->GetDictionary();
	CefRefPtr<CefListValue> servers = serversValue->GetList();

	if (settings.get() && servers.get() && servers->GetSize()) {
		if (settings->HasKey("maxBitsPerSecond") && settings->HasKey("serverTestDurationSeconds")) {
			int maxBitsPerSecond = settings->GetInt("maxBitsPerSecond");
			int serverTestDurationSeconds = settings->GetInt("serverTestDurationSeconds");

			m_last_test_servers.clear();

			for (int i = 0; i < servers->GetSize(); ++i) {
				CefRefPtr<CefDictionaryValue> server =
					servers->GetValue(i)->GetDictionary();

				if (server.get() && server->HasKey("url") && server->HasKey("streamKey")) {
					StreamElementsBandwidthTestClient::Server item;

					item.url = server->GetString("url");
					item.streamKey = server->GetString("streamKey");

					if (server->HasKey("useAuth") && server->GetBool("useAuth")) {
						item.useAuth = true;

						if (server->HasKey("authUsername")) {
							item.authUsername = server->GetString("authUsername");
						}

						if (server->HasKey("authPassword")) {
							item.authPassword = server->GetString("authPassword");
						}
					}

					m_last_test_servers.emplace_back(item);
				}
			}

			if (m_last_test_servers.size()) {
				m_isTestInProgress = true;

				// Signal test started
				DispatchJSEvent("hostBandwidthTestStarted", nullptr);

				m_client->TestMultipleServersBitsPerSecondAsync(
					m_last_test_servers,
					maxBitsPerSecond,
					nullptr,
					serverTestDurationSeconds,
					[](std::vector<StreamElementsBandwidthTestClient::Result>* results, void* data) {
						StreamElementsBandwidthTestManager* self = (StreamElementsBandwidthTestManager*)data;

						std::lock_guard<std::mutex> guard(self->m_mutex);

						// Copy
						self->m_last_test_results = *results;

						// Signal test completed
						DispatchJSEvent("hostBandwidthTestProgress", nullptr);
					},
					[](std::vector<StreamElementsBandwidthTestClient::Result>* results, void* data) {
						StreamElementsBandwidthTestManager* self = (StreamElementsBandwidthTestManager*)data;

						std::lock_guard<std::mutex> guard(self->m_mutex);

						// Copy
						self->m_last_test_results = *results;

						self->m_isTestInProgress = false;

						// Signal test completed
						DispatchJSEvent("hostBandwidthTestCompleted", nullptr);
					},
					this);
			}
		}
	}

	return m_isTestInProgress;
}

CefRefPtr<CefListValue> StreamElementsBandwidthTestManager::EndBandwidthTest(CefRefPtr<CefValue> options)
{
	CefRefPtr<CefListValue> list = CefListValue::Create();

	bool stopIfRunning = false;

	if (options.get() && options->GetType() == VT_BOOL) {
		stopIfRunning = options->GetBool();
	}

	m_client->CancelAll();

	for (auto result : m_last_test_results) {
		CefRefPtr<CefValue> itemValue = CefValue::Create();
		CefRefPtr<CefDictionaryValue> item = CefDictionaryValue::Create();
		itemValue->SetDictionary(item);

		item->SetBool("success", result.success);
		item->SetBool("wasCancelled", result.cancelled);
		item->SetString("serverUrl", result.serverUrl);
		item->SetString("streamKey", result.streamKey);
		item->SetInt("connectTimeMilliseconds", result.connectTimeMilliseconds);

		list->SetValue(list->GetSize(), itemValue);
	}

	return list;
}

CefRefPtr<CefDictionaryValue> StreamElementsBandwidthTestManager::GetBandwidthTestStatus()
{
	CefRefPtr<CefDictionaryValue> resultDictionary = CefDictionaryValue::Create();

	// Build servers list
	{
		CefRefPtr<CefValue> value = CefValue::Create();
		CefRefPtr<CefListValue> list = CefListValue::Create();
		value->SetList(list);

		for (auto server : m_last_test_servers) {
			CefRefPtr<CefValue> itemValue = CefValue::Create();
			CefRefPtr<CefDictionaryValue> item = CefDictionaryValue::Create();
			itemValue->SetDictionary(item);

			item->SetString("url", server.url);
			item->SetString("streamKey", server.streamKey);
			item->SetBool("useAuth", server.useAuth);
			item->SetString("authUsername", server.authUsername);

			list->SetValue(list->GetSize(), itemValue);
		}

		resultDictionary->SetValue("servers", value);
	}


	// Build results list
	{
		CefRefPtr<CefValue> value = CefValue::Create();
		CefRefPtr<CefListValue> list = CefListValue::Create();
		value->SetList(list);

		for (auto testResult : m_last_test_results) {
			CefRefPtr<CefValue> itemValue = CefValue::Create();
			CefRefPtr<CefDictionaryValue> item = CefDictionaryValue::Create();
			itemValue->SetDictionary(item);

			item->SetBool("success", testResult.success);
			item->SetBool("wasCancelled", testResult.cancelled);
			item->SetString("serverUrl", testResult.serverUrl);
			item->SetString("streamKey", testResult.streamKey);
			item->SetInt("connectTimeMilliseconds", testResult.connectTimeMilliseconds);

			list->SetValue(list->GetSize(), itemValue);
		}

		resultDictionary->SetValue("results", value);
	}

	return resultDictionary;
}

