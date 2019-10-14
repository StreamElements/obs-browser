#include "StreamElementsUtils.hpp"
#include "StreamElementsConfig.hpp"
#include "StreamElementsCefClient.hpp"
#include "Version.hpp"

#if CHROME_VERSION_BUILD >= 3729
#include <include/cef_api_hash.h>
#endif

#include <cstdint>
#include <codecvt>
#include <vector>
#include <regex>
#include <unordered_map>

#include <curl/curl.h>

#include <obs-frontend-api.h>

#include <QUrl>

#include <QFile>
#include <QDir>
#include <QUrl>
#include <regex>

#include "deps/picosha2/picosha2.h"

/* ========================================================= */

static const char *ENV_PRODUCT_NAME = "OBS.Live";

/* ========================================================= */

// convert wstring to UTF-8 string
static std::string wstring_to_utf8(const std::wstring &str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	return myconv.to_bytes(str);
}

std::string clean_guid_string(std::string input)
{
	return std::regex_replace(input, std::regex("-"), "");
}

static std::vector<std::string> tokenizeString(const std::string &str,
					       const std::string &delimiters)
{
	std::vector<std::string> tokens;
	// Skip delimiters at beginning.
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter".
	std::string::size_type pos = str.find_first_of(delimiters, lastPos);

	while (std::string::npos != pos ||
	       std::string::npos !=
		       lastPos) { // Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"
		pos = str.find_first_of(delimiters, lastPos);
	}
	return tokens;
}

/* ========================================================= */

void QtPostTask(std::function<void()> task)
{
	struct local_context {
		std::function<void()> task;
	};

	local_context *context = new local_context();
	context->task = task;

	QtPostTask(
		[](void *const data) {
			local_context *context = (local_context *)data;

			context->task();

			delete context;
		},
		context);
}

void QtPostTask(void (*func)(void *), void *const data)
{
	QTimer *t = new QTimer();
	t->moveToThread(qApp->thread());
	t->setSingleShot(true);
	QObject::connect(t, &QTimer::timeout, [=]() {
		t->deleteLater();

		func(data);
	});
	QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection,
				  Q_ARG(int, 0));
}

void QtExecSync(std::function<void()> task)
{
	struct local_context {
		std::function<void()> task;
	};

	local_context *context = new local_context();
	context->task = task;

	QtExecSync(
		[](void *data) {
			local_context *context = (local_context *)data;

			context->task();

			delete context;
		},
		context);
}

void QtExecSync(void (*func)(void *), void *const data)
{
	if (QThread::currentThread() == qApp->thread()) {
		func(data);
	} else {
		os_event_t *completeEvent;

		os_event_init(&completeEvent, OS_EVENT_TYPE_AUTO);

		QTimer *t = new QTimer();
		t->moveToThread(qApp->thread());
		t->setSingleShot(true);
		QObject::connect(t, &QTimer::timeout, [=]() {
			t->deleteLater();

			func(data);

			os_event_signal(completeEvent);
		});
		QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection,
					  Q_ARG(int, 0));

		QApplication::sendPostedEvents();

		os_event_wait(completeEvent);
		os_event_destroy(completeEvent);
	}
}

std::string DockWidgetAreaToString(const Qt::DockWidgetArea area)
{
	switch (area) {
	case Qt::LeftDockWidgetArea:
		return "left";
	case Qt::RightDockWidgetArea:
		return "right";
	case Qt::TopDockWidgetArea:
		return "top";
	case Qt::BottomDockWidgetArea:
		return "bottom";
	case Qt::NoDockWidgetArea:
	default:
		return "floating";
	}
}

std::string GetCommandLineOptionValue(const std::string key)
{
	QStringList args = QCoreApplication::instance()->arguments();

	std::string search = "--" + key + "=";

	for (int i = 0; i < args.size(); ++i) {
		std::string arg = args.at(i).toStdString();

		if (arg.substr(0, search.size()) == search) {
			return arg.substr(search.size());
		}
	}

	return std::string();
}

std::string LoadResourceString(std::string path)
{
	std::string result = "";

	QFile file(QString(path.c_str()));

	if (file.open(QFile::ReadOnly | QFile::Text)) {
		QTextStream stream(&file);

		result = stream.readAll().toStdString();
	}

	return result;
}

/* ========================================================= */

static uint64_t FromFileTime(const FILETIME &ft)
{
	ULARGE_INTEGER uli = {0};
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
	return uli.QuadPart;
}

void SerializeSystemTimes(CefRefPtr<CefValue> &output)
{
	SYNC_ACCESS();

	output->SetNull();

	FILETIME idleTime;
	FILETIME kernelTime;
	FILETIME userTime;

#ifdef _WIN32
	if (::GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		output->SetDictionary(d);

		static bool hasSavedStartingValues = false;
		static uint64_t savedIdleTime;
		static uint64_t savedKernelTime;
		static uint64_t savedUserTime;

		if (!hasSavedStartingValues) {
			savedIdleTime = FromFileTime(idleTime);
			savedKernelTime = FromFileTime(kernelTime);
			savedUserTime = FromFileTime(userTime);

			hasSavedStartingValues = true;
		}

		const uint64_t SECOND_PART = 10000000L;
		const uint64_t MS_PART = SECOND_PART / 1000L;

		uint64_t idleInt = FromFileTime(idleTime) - savedIdleTime;
		uint64_t kernelInt = FromFileTime(kernelTime) - savedKernelTime;
		uint64_t userInt = FromFileTime(userTime) - savedUserTime;

		uint64_t idleMs = idleInt / MS_PART;
		uint64_t kernelMs = kernelInt / MS_PART;
		uint64_t userMs = userInt / MS_PART;

		uint64_t idleSec = idleMs / (uint64_t)1000;
		uint64_t kernelSec = kernelMs / (uint64_t)1000;
		uint64_t userSec = userMs / (uint64_t)1000;

		uint64_t idleMod = idleMs % (uint64_t)1000;
		uint64_t kernelMod = kernelMs % (uint64_t)1000;
		uint64_t userMod = userMs % (uint64_t)1000;

		double idleRat = idleSec + ((double)idleMod / 1000.0);
		double kernelRat = kernelSec + ((double)kernelMod / 1000.0);
		double userRat = userSec + ((double)userMod / 1000.0);

		// https://msdn.microsoft.com/en-us/84f674e7-536b-4ae0-b523-6a17cb0a1c17
		// lpKernelTime [out, optional]
		// A pointer to a FILETIME structure that receives the amount of time that
		// the system has spent executing in Kernel mode (including all threads in
		// all processes, on all processors)
		//
		// >>> This time value also includes the amount of time the system has been idle.
		//

		d->SetDouble("idleSeconds", idleRat);
		d->SetDouble("kernelSeconds", kernelRat - idleRat);
		d->SetDouble("userSeconds", userRat);
		d->SetDouble("totalSeconds", kernelRat + userRat);
		d->SetDouble("busySeconds", kernelRat + userRat - idleRat);
	}
#endif
}

void SerializeSystemMemoryUsage(CefRefPtr<CefValue> &output)
{
	output->SetNull();

#ifdef _WIN32
	MEMORYSTATUSEX mem;

	mem.dwLength = sizeof(mem);

	if (GlobalMemoryStatusEx(&mem)) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		output->SetDictionary(d);

		const DWORDLONG DIV = 1048576;

		d->SetString("units", "MB");
		d->SetInt("memoryUsedPercentage", mem.dwMemoryLoad);
		d->SetInt("totalPhysicalMemory", mem.ullTotalPhys / DIV);
		d->SetInt("freePhysicalMemory", mem.ullAvailPhys / DIV);
		d->SetInt("totalVirtualMemory", mem.ullTotalVirtual / DIV);
		d->SetInt("freeVirtualMemory", mem.ullAvailVirtual / DIV);
		d->SetInt("freeExtendedVirtualMemory",
			  mem.ullAvailExtendedVirtual / DIV);
		d->SetInt("totalPageFileSize", mem.ullTotalPageFile / DIV);
		d->SetInt("freePageFileSize", mem.ullAvailPageFile / DIV);
	}
#endif
}

static CefString getRegStr(HKEY parent, const WCHAR *subkey, const WCHAR *key)
{
	CefString result;

	DWORD dataSize = 0;

	if (ERROR_SUCCESS == ::RegGetValueW(parent, subkey, key, RRF_RT_ANY,
					    NULL, NULL, &dataSize)) {
		WCHAR *buffer = new WCHAR[dataSize];

		if (ERROR_SUCCESS == ::RegGetValueW(parent, subkey, key,
						    RRF_RT_ANY, NULL, buffer,
						    &dataSize)) {
			result = buffer;
		}

		delete[] buffer;
	}

	return result;
};

static DWORD getRegDWORD(HKEY parent, const WCHAR *subkey, const WCHAR *key)
{
	DWORD result = 0;
	DWORD dataSize = sizeof(DWORD);

	::RegGetValueW(parent, subkey, key, RRF_RT_DWORD, NULL, &result,
		       &dataSize);

	return result;
}

void SerializeSystemHardwareProperties(CefRefPtr<CefValue> &output)
{
	output->SetNull();

#ifdef _WIN32
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
	output->SetDictionary(d);

	SYSTEM_INFO info;

	::GetNativeSystemInfo(&info);

	switch (info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL:
		d->SetString("cpuArch", "x86");
		break;
	case PROCESSOR_ARCHITECTURE_IA64:
		d->SetString("cpuArch", "IA64");
		break;
	case PROCESSOR_ARCHITECTURE_AMD64:
		d->SetString("cpuArch", "x64");
		break;
	case PROCESSOR_ARCHITECTURE_ARM:
		d->SetString("cpuArch", "ARM");
		break;
	case PROCESSOR_ARCHITECTURE_ARM64:
		d->SetString("cpuArch", "ARM64");
		break;

	default:
	case PROCESSOR_ARCHITECTURE_UNKNOWN:
		d->SetString("cpuArch", "Unknown");
		break;
	}

	d->SetInt("cpuCount", info.dwNumberOfProcessors);
	d->SetInt("cpuLevel", info.wProcessorLevel);

	/*
	CefRefPtr<CefDictionaryValue> f = CefDictionaryValue::Create();

	f->SetBool("PF_3DNOW_INSTRUCTIONS_AVAILABLE", ::IsProcessorFeaturePresent(PF_3DNOW_INSTRUCTIONS_AVAILABLE));
	f->SetBool("PF_CHANNELS_ENABLED", ::IsProcessorFeaturePresent(PF_CHANNELS_ENABLED));
	f->SetBool("PF_COMPARE_EXCHANGE_DOUBLE", ::IsProcessorFeaturePresent(PF_COMPARE_EXCHANGE_DOUBLE));
	f->SetBool("PF_COMPARE_EXCHANGE128", ::IsProcessorFeaturePresent(PF_COMPARE_EXCHANGE128));
	f->SetBool("PF_COMPARE64_EXCHANGE128", ::IsProcessorFeaturePresent(PF_COMPARE64_EXCHANGE128));
	f->SetBool("PF_FASTFAIL_AVAILABLE", ::IsProcessorFeaturePresent(PF_FASTFAIL_AVAILABLE));
	f->SetBool("PF_FLOATING_POINT_EMULATED", ::IsProcessorFeaturePresent(PF_FLOATING_POINT_EMULATED));
	f->SetBool("PF_FLOATING_POINT_PRECISION_ERRATA", ::IsProcessorFeaturePresent(PF_FLOATING_POINT_PRECISION_ERRATA));
	f->SetBool("PF_MMX_INSTRUCTIONS_AVAILABLE", ::IsProcessorFeaturePresent(PF_MMX_INSTRUCTIONS_AVAILABLE));
	f->SetBool("PF_NX_ENABLED", ::IsProcessorFeaturePresent(PF_NX_ENABLED));
	f->SetBool("PF_PAE_ENABLED", ::IsProcessorFeaturePresent(PF_PAE_ENABLED));
	f->SetBool("PF_RDTSC_INSTRUCTION_AVAILABLE", ::IsProcessorFeaturePresent(PF_RDTSC_INSTRUCTION_AVAILABLE));
	f->SetBool("PF_RDWRFSGSBASE_AVAILABLE", ::IsProcessorFeaturePresent(PF_RDWRFSGSBASE_AVAILABLE));
	f->SetBool("PF_SECOND_LEVEL_ADDRESS_TRANSLATION", ::IsProcessorFeaturePresent(PF_SECOND_LEVEL_ADDRESS_TRANSLATION));
	f->SetBool("PF_SSE3_INSTRUCTIONS_AVAILABLE", ::IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE));
	f->SetBool("PF_VIRT_FIRMWARE_ENABLED", ::IsProcessorFeaturePresent(PF_VIRT_FIRMWARE_ENABLED));
	f->SetBool("PF_XMMI_INSTRUCTIONS_AVAILABLE", ::IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE));
	f->SetBool("PF_XMMI64_INSTRUCTIONS_AVAILABLE", ::IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE));
	f->SetBool("PF_XSAVE_ENABLED", ::IsProcessorFeaturePresent(PF_XSAVE_ENABLED));

	CefRefPtr<CefValue> featuresValue = CefValue::Create();
	featuresValue->SetDictionary(f);
	d->SetValue("cpuFeatures", featuresValue);*/

	{
		CefRefPtr<CefListValue> cpuList = CefListValue::Create();

		HKEY hRoot;
		if (ERROR_SUCCESS ==
		    ::RegOpenKeyA(
			    HKEY_LOCAL_MACHINE,
			    "HARDWARE\\DESCRIPTION\\System\\CentralProcessor",
			    &hRoot)) {
			WCHAR cpuKeyBuffer[2048];

			for (DWORD index = 0;
			     ERROR_SUCCESS ==
			     ::RegEnumKeyW(hRoot, index, cpuKeyBuffer,
					   sizeof(cpuKeyBuffer));
			     ++index) {
				CefRefPtr<CefDictionaryValue> p =
					CefDictionaryValue::Create();

				p->SetString("name",
					     getRegStr(hRoot, cpuKeyBuffer,
						       L"ProcessorNameString"));
				p->SetString("vendor",
					     getRegStr(hRoot, cpuKeyBuffer,
						       L"VendorIdentifier"));
				p->SetInt("speedMHz",
					  getRegDWORD(hRoot, cpuKeyBuffer,
						      L"~MHz"));
				p->SetString("identifier",
					     getRegStr(hRoot, cpuKeyBuffer,
						       L"Identifier"));

				cpuList->SetDictionary(cpuList->GetSize(), p);
			}

			::RegCloseKey(hRoot);
		}

		d->SetList("cpuHardware", cpuList);
	}

	{
		CefRefPtr<CefDictionaryValue> bios =
			CefDictionaryValue::Create();

		HKEY hRoot;
		if (ERROR_SUCCESS ==
		    ::RegOpenKeyW(HKEY_LOCAL_MACHINE,
				  L"HARDWARE\\DESCRIPTION\\System", &hRoot)) {
			HKEY hBios;
			if (ERROR_SUCCESS ==
			    ::RegOpenKeyW(hRoot, L"BIOS", &hBios)) {
				WCHAR subKeyBuffer[2048];
				DWORD bufSize = sizeof(subKeyBuffer) /
						sizeof(subKeyBuffer[0]);

				DWORD valueIndex = 0;
				DWORD valueType = 0;

				LSTATUS callStatus = ::RegEnumValueW(
					hBios, valueIndex, subKeyBuffer,
					&bufSize, NULL, &valueType, NULL, NULL);
				while (ERROR_NO_MORE_ITEMS != callStatus) {
					switch (valueType) {
					case REG_DWORD_BIG_ENDIAN:
					case REG_DWORD_LITTLE_ENDIAN:
						bios->SetInt(
							subKeyBuffer,
							getRegDWORD(
								hRoot, L"BIOS",
								subKeyBuffer));
						break;

					case REG_QWORD:
						bios->SetInt(
							subKeyBuffer,
							getRegDWORD(
								hRoot, L"BIOS",
								subKeyBuffer));
						break;

					case REG_SZ:
					case REG_EXPAND_SZ:
					case REG_MULTI_SZ:
						bios->SetString(
							subKeyBuffer,
							getRegStr(
								hRoot, L"BIOS",
								subKeyBuffer));
						break;
					}

					++valueIndex;

					bufSize = sizeof(subKeyBuffer) /
						  sizeof(subKeyBuffer[0]);
					callStatus = ::RegEnumValueW(
						hBios, valueIndex, subKeyBuffer,
						&bufSize, NULL, &valueType,
						NULL, NULL);
				}

				::RegCloseKey(hBios);
			}

			::RegCloseKey(hRoot);
		}

		d->SetDictionary("bios", bios);
	}
#endif
}

/* ========================================================= */

void SerializeAvailableInputSourceTypes(CefRefPtr<CefValue> &output)
{
	// Response codec collection (array)
	CefRefPtr<CefListValue> list = CefListValue::Create();

	// Response codec collection is our root object
	output->SetList(list);

	// Iterate over all input sources
	bool continue_iteration = true;
	for (size_t idx = 0; continue_iteration; ++idx) {
		// Filled by obs_enum_input_types() call below
		const char *sourceId;

		// Get next input source type, obs_enum_input_types() returns true as long as
		// there is data at the specified index
		continue_iteration = obs_enum_input_types(idx, &sourceId);

		if (continue_iteration) {
			// Get source caps
			uint32_t sourceCaps =
				obs_get_source_output_flags(sourceId);

			// If source has video
			if ((sourceCaps & OBS_SOURCE_VIDEO) ==
			    OBS_SOURCE_VIDEO) {
				// Create source response dictionary
				CefRefPtr<CefDictionaryValue> dic =
					CefDictionaryValue::Create();

				// Set codec dictionary properties
				dic->SetString("id", sourceId);
				dic->SetString(
					"name",
					obs_source_get_display_name(sourceId));
				dic->SetBool("hasVideo",
					     (sourceCaps & OBS_SOURCE_VIDEO) ==
						     OBS_SOURCE_VIDEO);
				dic->SetBool("hasAudio",
					     (sourceCaps & OBS_SOURCE_AUDIO) ==
						     OBS_SOURCE_AUDIO);

				// Compare sourceId to known video capture devices
				dic->SetBool(
					"isVideoCaptureDevice",
					strcmp(sourceId, "dshow_input") == 0 ||
						strcmp(sourceId,
						       "decklink-input") == 0);

				// Compare sourceId to known game capture source
				dic->SetBool("isGameCaptureDevice",
					     strcmp(sourceId, "game_capture") ==
						     0);

				// Compare sourceId to known browser source
				dic->SetBool("isBrowserSource",
					     strcmp(sourceId,
						    "browser_source") == 0);

				// Append dictionary to response list
				list->SetDictionary(list->GetSize(), dic);
			}
		}
	}
}

std::string SerializeAppStyleSheet()
{
	std::string result = qApp->styleSheet().toStdString();

	if (result.compare(0, 8, "file:///") == 0) {
		QUrl url(result.c_str());

		if (url.isLocalFile()) {
			result = result.substr(8);

			QFile file(result.c_str());

			if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
				QTextStream in(&file);

				result = in.readAll().toStdString();
			}
		}
	}

	return result;
}

std::string GetAppStyleSheetSelectorContent(std::string selector)
{
	std::string result;

	std::string css = SerializeAppStyleSheet();

	std::replace(css.begin(), css.end(), '\n', ' ');

	css = std::regex_replace(css, std::regex("/\\*.*?\\*/"), "");

	std::regex selector_regex("[^\\s]*" + selector + "[\\s]*\\{(.*?)\\}");
	std::smatch selector_match;

	if (std::regex_search(css, selector_match, selector_regex)) {
		result = std::string(selector_match[1].first,
				     selector_match[1].second);
	}

	return result;
}

std::string GetCurrentThemeName()
{
	std::string result;

	config_t *globalConfig =
		obs_frontend_get_global_config(); // does not increase refcount

	const char *themeName =
		config_get_string(globalConfig, "General", "CurrentTheme");
	if (!themeName) {
		/* Use deprecated "Theme" value if available */
		themeName = config_get_string(globalConfig, "General", "Theme");
		if (!themeName) {
			themeName = "Default";
		}

		result = themeName;
	}

	std::string appStyle = qApp->styleSheet().toStdString();

	if (appStyle.substr(0, 7) == "file://") {
		QUrl url(appStyle.c_str());

		if (url.isLocalFile()) {
			result = url.fileName().split('.')[0].toStdString();
		}
	}

	return result;
}

std::string GetCefVersionString()
{
	char buf[64];

	sprintf(buf, "cef.%d.%d.chrome.%d.%d.%d.%d", cef_version_info(0),
		cef_version_info(1), cef_version_info(2), cef_version_info(3),
		cef_version_info(4), cef_version_info(5));

	return std::string(buf);
}

std::string GetCefPlatformApiHash()
{
	return cef_api_hash(0);
}

std::string GetCefUniversalApiHash()
{
	return cef_api_hash(1);
}

std::string GetStreamElementsPluginVersionString()
{
	char version_buf[64];
	sprintf(version_buf, "%d.%d.%d.%d",
		(int)((STREAMELEMENTS_PLUGIN_VERSION % 1000000000000L) /
		      10000000000L),
		(int)((STREAMELEMENTS_PLUGIN_VERSION % 10000000000L) /
		      100000000L),
		(int)((STREAMELEMENTS_PLUGIN_VERSION % 100000000L) / 1000000L),
		(int)(STREAMELEMENTS_PLUGIN_VERSION % 1000000L));

	return version_buf;
}

std::string GetStreamElementsApiVersionString()
{
	char version_buf[64];

	sprintf(version_buf, "%d.%d", HOST_API_VERSION_MAJOR,
		HOST_API_VERSION_MINOR);

	return version_buf;
}

/* ========================================================= */

#include <winhttp.h>
#pragma comment(lib, "Winhttp.lib")
void SetGlobalCURLOptions(CURL *curl, const char *url)
{
	std::string proxy =
		GetCommandLineOptionValue("streamelements-http-proxy");

	if (!proxy.size()) {
#ifdef _WIN32
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG config;

		if (WinHttpGetIEProxyConfigForCurrentUser(&config)) {
			// http=127.0.0.1:8888;https=127.0.0.1:8888
			if (config.lpszProxy) {
				proxy = wstring_to_utf8(config.lpszProxy);

				std::map<std::string, std::string> schemes;
				for (auto kvstr : tokenizeString(proxy, ";")) {
					std::vector<std::string> kv =
						tokenizeString(kvstr, "=");

					if (kv.size() == 2) {
						std::transform(kv[0].begin(),
							       kv[0].end(),
							       kv[0].begin(),
							       tolower);
						schemes[kv[0]] = kv[1];
					}
				}

				std::string scheme =
					tokenizeString(url, ":")[0];
				std::transform(scheme.begin(), scheme.end(),
					       scheme.begin(), tolower);

				if (schemes.count(scheme)) {
					proxy = schemes[scheme];
				} else if (schemes.count("http")) {
					proxy = schemes["http"];
				} else {
					proxy = "";
				}
			}

			if (config.lpszProxy) {
				GlobalFree((HGLOBAL)config.lpszProxy);
			}

			if (config.lpszProxyBypass) {
				GlobalFree((HGLOBAL)config.lpszProxyBypass);
			}

			if (config.lpszAutoConfigUrl) {
				GlobalFree((HGLOBAL)config.lpszAutoConfigUrl);
			}
		}
#endif
	}

	if (proxy.size()) {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
	}
}

struct http_callback_context {
	http_client_callback_t callback;
	void *userdata;
};
static size_t http_write_callback(char *ptr, size_t size, size_t nmemb,
				  void *userdata)
{
	http_callback_context *context = (http_callback_context *)userdata;

	bool result = true;
	if (context->callback) {
		result = context->callback(ptr, size * nmemb, context->userdata,
					   nullptr, 0);
	}

	if (result) {
		return size * nmemb;
	} else {
		return 0;
	}
};

bool HttpGet(const char *url, http_client_request_headers_t request_headers,
	     http_client_callback_t callback, void *userdata)
{
	bool result = false;

	CURL *curl = curl_easy_init();

	if (curl) {
		SetGlobalCURLOptions(curl, url);

		curl_easy_setopt(curl, CURLOPT_URL, url);

		curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 512L * 1024L);

		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

		http_callback_context context;
		context.callback = callback;
		context.userdata = userdata;

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
				 http_write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);

		curl_slist *headers = NULL;
		for (auto h : request_headers) {
			headers = curl_slist_append(
				headers, (h.first + ": " + h.second).c_str());
		}

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		char *errorbuf = new char[CURL_ERROR_SIZE];
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuf);

		CURLcode res = curl_easy_perform(curl);

		curl_slist_free_all(headers);

		if (CURLE_OK == res) {
			result = true;
		}

		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

		if (callback) {
			if (!result) {
				callback(nullptr, 0, 0, errorbuf,
					 (int)http_code);
			} else {
				callback(nullptr, 0, 0, nullptr,
					 (int)http_code);
			}
		}

		delete[] errorbuf;

		curl_easy_cleanup(curl);
	}

	return result;
}

bool HttpPost(const char *url, http_client_request_headers_t request_headers,
	      void *buffer, size_t buffer_len, http_client_callback_t callback,
	      void *userdata)
{
	bool result = false;

	CURL *curl = curl_easy_init();

	if (curl) {
		SetGlobalCURLOptions(curl, url);

		curl_easy_setopt(curl, CURLOPT_URL, url);

		curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 512L * 1024L);

		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

		http_callback_context context;
		context.callback = callback;
		context.userdata = userdata;

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
				 http_write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);

		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)buffer_len);
		curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, buffer);

		curl_slist *headers = NULL;
		for (auto h : request_headers) {
			headers = curl_slist_append(
				headers, (h.first + ": " + h.second).c_str());
		}

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		char *errorbuf = new char[CURL_ERROR_SIZE];
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuf);

		CURLcode res = curl_easy_perform(curl);

		curl_slist_free_all(headers);

		if (CURLE_OK == res) {
			result = true;
		}

		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

		if (callback) {
			if (!result) {
				callback(nullptr, 0, 0, errorbuf,
					 (int)http_code);
			} else {
				callback(nullptr, 0, 0, nullptr,
					 (int)http_code);
			}
		}

		delete[] errorbuf;

		curl_easy_cleanup(curl);
	}

	return result;
}

static const size_t MAX_HTTP_STRING_RESPONSE_LENGTH = 1024 * 1024 * 100;

bool HttpGetString(const char *url,
		   http_client_request_headers_t request_headers,
		   http_client_string_callback_t callback, void *userdata)
{
	std::vector<char> buffer;
	std::string error = "";
	int http_status_code = 0;

	auto cb = [&](void *data, size_t datalen, void *userdata,
		      char *error_msg, int http_code) -> bool {
		if (http_code != 0) {
			http_status_code = http_code;
		}

		if (error_msg) {
			error = error_msg;

			return false;
		}

		char *in = (char *)data;

		std::copy(in, in + datalen, std::back_inserter(buffer));

		return buffer.size() < MAX_HTTP_STRING_RESPONSE_LENGTH;
	};

	bool success = HttpGet(url, request_headers, cb, nullptr);

	buffer.push_back(0);

	callback((char *)&buffer[0], userdata,
		 error.size() ? (char *)error.c_str() : nullptr,
		 http_status_code);

	return success;
}

bool HttpPostString(const char *url,
		    http_client_request_headers_t request_headers,
		    const char *postData,
		    http_client_string_callback_t callback, void *userdata)
{
	std::vector<char> buffer;
	std::string error = "";
	int http_status_code = 0;

	auto cb = [&](void *data, size_t datalen, void *userdata,
		      char *error_msg, int http_code) -> bool {
		if (http_code != 0) {
			http_status_code = http_code;
		}

		if (error_msg) {
			error = error_msg;

			return false;
		}

		char *in = (char *)data;

		std::copy(in, in + datalen, std::back_inserter(buffer));

		return buffer.size() < MAX_HTTP_STRING_RESPONSE_LENGTH;
	};

	bool success = HttpPost(url, request_headers, (void *)postData,
				strlen(postData), cb, nullptr);

	buffer.push_back(0);

	callback((char *)&buffer[0], userdata,
		 error.size() ? (char *)error.c_str() : nullptr,
		 http_status_code);

	return success;
}

/* ========================================================= */

static std::string GetEnvironmentConfigRegKeyPath(const char *productName)
{
#ifdef _WIN64
	std::string REG_KEY_PATH = "SOFTWARE\\WOW6432Node\\StreamElements";
#else
	std::string REG_KEY_PATH = "SOFTWARE\\StreamElements";
#endif

	if (productName && productName[0]) {
		REG_KEY_PATH += "\\";
		REG_KEY_PATH += productName;
	}

	return REG_KEY_PATH;
}

static std::string ReadEnvironmentConfigString(const char *regValueName,
					       const char *productName)
{
	std::string result = "";

	std::string REG_KEY_PATH = GetEnvironmentConfigRegKeyPath(productName);

	DWORD bufLen = 16384;
	char *buffer = new char[bufLen];

	LSTATUS lResult = RegGetValueA(HKEY_LOCAL_MACHINE, REG_KEY_PATH.c_str(),
				       regValueName, RRF_RT_REG_SZ, NULL,
				       buffer, &bufLen);

	if (ERROR_SUCCESS == lResult) {
		result = buffer;
	}

	delete[] buffer;

	return result;
}

bool WriteEnvironmentConfigString(const char *regValueName,
				  const char *regValue, const char *productName)
{
	bool result = false;

	std::string REG_KEY_PATH = GetEnvironmentConfigRegKeyPath(productName);

	LSTATUS lResult = RegSetKeyValueA(HKEY_LOCAL_MACHINE,
					  REG_KEY_PATH.c_str(), regValueName,
					  REG_SZ, regValue, strlen(regValue));

	if (lResult != ERROR_SUCCESS) {
		result = WriteEnvironmentConfigStrings(
			{{productName ? productName : "", regValueName,
			  regValue}});
	} else {
		result = true;
	}

	return result;
}

#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
static std::wstring GetCurrentDllFolderPathW()
{
	std::wstring result = L"";

	WCHAR path[MAX_PATH];
	HMODULE hModule;

	if (GetModuleHandleExW(
		    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		    (LPWSTR)&GetCurrentDllFolderPathW, &hModule)) {
		GetModuleFileNameW(hModule, path, sizeof(path));
		PathRemoveFileSpecW(path);

		result = std::wstring(path);

		if (!result.empty() && result[result.size() - 1] != '\\')
			result += L"\\";
	}

	return result;
}

bool WriteEnvironmentConfigStrings(streamelements_env_update_requests requests)
{
	std::vector<std::string> args;

	for (auto req : requests) {
		if (req.product.size()) {
			args.push_back(req.product + "/" + req.key + "=" +
				       req.value);
		} else {
			args.push_back(req.key + "=" + req.value);
		}
	}

	STARTUPINFOW startInf;
	memset(&startInf, 0, sizeof startInf);
	startInf.cb = sizeof(startInf);

	PROCESS_INFORMATION procInf;
	memset(&procInf, 0, sizeof procInf);

	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	std::wstring wArgs;
	for (auto arg : args) {
		wArgs += L"\"" + myconv.from_bytes(arg) + L"\" ";
	}
	if (!wArgs.empty()) {
		// Remove trailing space
		wArgs = wArgs.substr(0, wArgs.size() - 1);
	}

	std::wstring wExePath = GetCurrentDllFolderPathW() +
				L"obs-streamelements-set-machine-config.exe";

	HINSTANCE hInst = ShellExecuteW(NULL, L"runas", wExePath.c_str(),
					wArgs.c_str(), NULL, SW_SHOW);

	BOOL bResult = hInst > (HINSTANCE)32;

	return bResult;
}

std::string ReadProductEnvironmentConfigurationString(const char *key)
{
	return ReadEnvironmentConfigString(key, ENV_PRODUCT_NAME);
}

bool WriteProductEnvironmentConfigurationString(const char *key,
						const char *value)
{
	return WriteEnvironmentConfigString(key, value, ENV_PRODUCT_NAME);
}

bool WriteProductEnvironmentConfigurationStrings(
	streamelements_env_update_requests requests)
{
	for (int i = 0; i < requests.size(); ++i) {
		requests[i].product = ENV_PRODUCT_NAME;
	}

	return WriteEnvironmentConfigStrings(requests);
}

/* ========================================================= */

std::string CreateGloballyUniqueIdString()
{
	std::string result;

	const int GUID_STRING_LENGTH = 39;

	GUID guid;
	CoCreateGuid(&guid);

	OLECHAR guidStr[GUID_STRING_LENGTH];
	StringFromGUID2(guid, guidStr, GUID_STRING_LENGTH);

	guidStr[GUID_STRING_LENGTH - 2] = 0;
	result = wstring_to_utf8(guidStr + 1);

	return result;
}

#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
static std::string CreateCryptoSecureRandomNumberString()
{
	std::string result = "0";

	BCRYPT_ALG_HANDLE hAlgo;

	if (0 == BCryptOpenAlgorithmProvider(&hAlgo, BCRYPT_RNG_ALGORITHM, NULL,
					     0)) {
		uint64_t buffer;

		if (0 == BCryptGenRandom(hAlgo, (PUCHAR)&buffer, sizeof(buffer),
					 0)) {
			char buf[sizeof(buffer) * 2 + 1];
			sprintf_s(buf, sizeof(buf), "%llX", buffer);

			result = buf;

			std::cout << buffer << std::endl;
		}

		BCryptCloseAlgorithmProvider(hAlgo, 0);
	}

	return result;
}

#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "Advapi32.lib")
std::string GetComputerSystemUniqueId()
{
	const char *REG_VALUE_NAME = "MachineUniqueIdentifier";

	std::string result =
		ReadEnvironmentConfigString(REG_VALUE_NAME, nullptr);
	std::string prevResult = result;

	if (result.size()) {
		// Discard invalid values
		if (result ==
			    "WUID/03000200-0400-0500-0006-000700080009" || // Known duplicate
		    result ==
			    "WUID/00000000-0000-0000-0000-000000000000" || // Null value
		    result ==
			    "WUID/FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF" || // Invalid value
		    result ==
			    "WUID/00412F4E-0000-0000-0000-0000FFFFFFFF") { // Set by russian MS Office crack
			result = "";
		}
	}

	if (!result.size()) {
		// Get unique ID from WMI

		HRESULT hr = CoInitialize(NULL);

		// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/initializing-com-for-a-wmi-application
		if (SUCCEEDED(hr)) {
			bool uinitializeCom = hr == S_OK;

			// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/setting-the-default-process-security-level-using-c-
			CoInitializeSecurity(
				NULL, // security descriptor
				-1,   // use this simple setting
				NULL, // use this simple setting
				NULL, // reserved
				RPC_C_AUTHN_LEVEL_DEFAULT, // authentication level
				RPC_C_IMP_LEVEL_IMPERSONATE, // impersonation level
				NULL,      // use this simple setting
				EOAC_NONE, // no special capabilities
				NULL);     // reserved

			IWbemLocator *pLocator;

			// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/creating-a-connection-to-a-wmi-namespace
			hr = CoCreateInstance(CLSID_WbemLocator, 0,
					      CLSCTX_INPROC_SERVER,
					      IID_IWbemLocator,
					      (LPVOID *)&pLocator);

			if (SUCCEEDED(hr)) {
				IWbemServices *pSvc = 0;

				// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/creating-a-connection-to-a-wmi-namespace
				hr = pLocator->ConnectServer(
					BSTR(L"root\\cimv2"), //namespace
					NULL,                 // User name
					NULL,                 // User password
					0,                    // Locale
					NULL,                 // Security flags
					0,                    // Authority
					0,                    // Context object
					&pSvc); // IWbemServices proxy

				if (SUCCEEDED(hr)) {
					hr = CoSetProxyBlanket(
						pSvc, RPC_C_AUTHN_WINNT,
						RPC_C_AUTHZ_NONE, NULL,
						RPC_C_AUTHN_LEVEL_CALL,
						RPC_C_IMP_LEVEL_IMPERSONATE,
						NULL, EOAC_NONE);

					if (SUCCEEDED(hr)) {
						IEnumWbemClassObject
							*pEnumerator = NULL;

						hr = pSvc->ExecQuery(
							(BSTR)L"WQL",
							(BSTR)L"select * from Win32_ComputerSystemProduct",
							WBEM_FLAG_FORWARD_ONLY,
							NULL, &pEnumerator);

						if (SUCCEEDED(hr)) {
							IWbemClassObject *pObj =
								NULL;

							ULONG resultCount;
							hr = pEnumerator->Next(
								WBEM_INFINITE,
								1, &pObj,
								&resultCount);

							if (SUCCEEDED(hr)) {
								VARIANT value;

								hr = pObj->Get(
									L"UUID",
									0,
									&value,
									NULL,
									NULL);

								if (SUCCEEDED(
									    hr)) {
									if (value.vt !=
									    VT_NULL) {
										result =
											std::string(
												"SWID/") +
											clean_guid_string(wstring_to_utf8(
												std::wstring(
													value.bstrVal)));
										result +=
											"-";
										result += clean_guid_string(
											CreateGloballyUniqueIdString());
										result +=
											"-";
										result +=
											CreateCryptoSecureRandomNumberString();
									}
									VariantClear(
										&value);
								}
							}

							pEnumerator->Release();
						}
					}

					pSvc->Release();
				}

				pLocator->Release();
			}

			if (uinitializeCom) {
				CoUninitialize();
			}
		}
	}

	if (!result.size()) {
		// Failed retrieving UUID, generate our own
		result = std::string("SEID/") +
			 clean_guid_string(CreateGloballyUniqueIdString());
		result += "-";
		result += CreateCryptoSecureRandomNumberString();
	}

	if (result.size() && result != prevResult) {
		// Save for future use
		WriteEnvironmentConfigString(REG_VALUE_NAME, result.c_str(),
					     nullptr);
	}

	return result;
}

bool ParseQueryString(std::string input,
		      std::map<std::string, std::string> &result)
{
	std::string s = input;

	while (s.size()) {
		std::string left;

		size_t offset = s.find('&');
		if (offset != std::string::npos) {
			left = s.substr(0, offset);
			s = s.substr(offset + 1);
		} else {
			left = s;
			s = "";
		}

		std::string right = "";
		offset = left.find('=');

		if (offset != std::string::npos) {
			right = left.substr(offset + 1);
			left = left.substr(0, offset);
		}

		result[left] = right;
	}

	return true;
}

std::string CreateSHA256Digest(std::string &input)
{
	std::vector<unsigned char> hash(picosha2::k_digest_size);
	picosha2::hash256(input.begin(), input.end(), hash.begin(), hash.end());

	return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
}

static std::mutex s_session_message_signature_mutex;
static std::string s_session_message_signature_random = "";

std::string CreateSessionMessageSignature(std::string &message)
{
	if (!s_session_message_signature_random.size()) {
		std::lock_guard<std::mutex> guard(
			s_session_message_signature_mutex);
		if (!s_session_message_signature_random.size()) {
			s_session_message_signature_random =
				CreateCryptoSecureRandomNumberString();
		}
	}

	std::string digest_input = message + s_session_message_signature_random;

	return CreateSHA256Digest(digest_input);
}

bool VerifySessionMessageSignature(std::string &message, std::string &signature)
{
	std::string digest_input = message + s_session_message_signature_random;

	std::string digest = CreateSHA256Digest(digest_input);

	return digest == signature;
}

std::string CreateSessionSignedAbsolutePathURL(std::wstring path)
{
	CefURLParts parts;

	path = std::regex_replace(path, std::wregex(L"#"), L"%23");
	path = std::regex_replace(path, std::wregex(L"&"), L"%26");

	CefString(&parts.scheme) = "https";
	CefString(&parts.host) = "absolute";
	CefString(&parts.path) = std::wstring(L"/") + path;

	CefString url;
	CefCreateURL(parts, url);
	CefParseURL(url, parts);

	std::string message = CefString(&parts.path).ToString();

	message =
		CefURIDecode(message, true, cef_uri_unescape_rule_t::UU_SPACES);
	message = CefURIDecode(
		message, true,
		cef_uri_unescape_rule_t::
			UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

	message = message.erase(0, 1);

	//message = std::regex_replace(message, std::regex("#"), "%23");
	//message = std::regex_replace(message, std::regex("&"), "%26");

	return url.ToString() + std::string("?digest=") +
	       CreateSessionMessageSignature(message);
}

bool VerifySessionSignedAbsolutePathURL(std::string url, std::string &path)
{
	CefURLParts parts;

	if (!CefParseURL(CefString(url), parts)) {
		return false;
	} else {
		path = CefString(&parts.path).ToString();

		path = CefURIDecode(path, true,
				    cef_uri_unescape_rule_t::UU_SPACES);
		path = CefURIDecode(
			path, true,
			cef_uri_unescape_rule_t::
				UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

		path = path.erase(0, 1);

		std::map<std::string, std::string> queryArgs;
		ParseQueryString(CefString(&parts.query), queryArgs);

		if (!queryArgs.count("digest")) {
			return false;
		} else {
			std::string signature = queryArgs["digest"];

			std::string message = path;

			//message = std::regex_replace(message, std::regex("#"), "%23");
			//message = std::regex_replace(message, std::regex("&"), "%26");

			return VerifySessionMessageSignature(message,
							     signature);
		}
	}
}

/* ========================================================= */

bool IsAlwaysOnTop(QWidget *window)
{
#ifdef WIN32
	DWORD exStyle = GetWindowLong((HWND)window->winId(), GWL_EXSTYLE);
	return (exStyle & WS_EX_TOPMOST) != 0;
#else
	return (window->windowFlags() & Qt::WindowStaysOnTopHint) != 0;
#endif
}

void SetAlwaysOnTop(QWidget *window, bool enable)
{
#ifdef WIN32
	HWND hwnd = (HWND)window->winId();
	SetWindowPos(hwnd, enable ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
		     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
	Qt::WindowFlags flags = window->windowFlags();

	if (enable)
		flags |= Qt::WindowStaysOnTopHint;
	else
		flags &= ~Qt::WindowStaysOnTopHint;

	window->setWindowFlags(flags);
	window->show();
#endif
}

double GetObsGlobalFramesPerSecond()
{
	config_t *basicConfig =
		obs_frontend_get_profile_config(); // does not increase refcount

	switch (config_get_uint(basicConfig, "Video", "FPSType")) {
	case 0: // Common
		return (double)atoi(
			config_get_string(basicConfig, "Video", "FPSCommon"));
		break;

	case 1: // Integer
		return (double)config_get_uint(basicConfig, "Video", "FPSInt");
		break;

	case 2: // Fractional
		return (double)config_get_uint(basicConfig, "Video", "FPSNum") /
		       (double)config_get_uint(basicConfig, "Video", "FPSDen");
		break;
	}
}

void AdviseHostUserInterfaceStateChanged()
{
	static std::mutex mutex;

	static QTimer *t = nullptr;

	std::lock_guard<std::mutex> guard(mutex);

	if (t == nullptr) {
		t = new QTimer();

		t->moveToThread(qApp->thread());
		t->setSingleShot(true);

		QObject::connect(t, &QTimer::timeout, [&]() {
			std::lock_guard<std::mutex> guard(mutex);

			t->deleteLater();
			t = nullptr;

			// Advise guest code of user interface state changes
			StreamElementsCefClient::DispatchJSEvent(
				"hostUserInterfaceStateChanged", "null");
		});
	}

	QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection,
				  Q_ARG(int, 250));
}

bool ParseStreamElementsOverlayURL(std::string url, std::string &overlayId,
				   std::string &accountId)
{
	std::regex url_regex(
		"^https://streamelements.com/overlay/([^/]+)/([^/]+)$");
	std::smatch match;

	if (std::regex_match(url, match, url_regex)) {
		overlayId = match[1].str();
		accountId = match[2].str();

		return true;
	}

	return false;
}

std::string GetStreamElementsOverlayEditorURL(std::string overlayId,
					      std::string accountId)
{
	return std::string("https://streamelements.com/overlay/") + overlayId +
	       std::string("/editor");
}

#if ENABLE_DECRYPT_COOKIES
#include "deps/sqlite/sqlite3.h"
#include <windows.h>
#include <wincrypt.h>
#include <dpapi.h>
#pragma comment(lib, "crypt32.lib")
void StreamElementsDecryptCefCookiesFile(const char *path_utf8)
{
#ifdef _WIN32
	sqlite3 *db;

	if (SQLITE_OK !=
	    sqlite3_open_v2(path_utf8, &db, SQLITE_OPEN_READWRITE, nullptr)) {
		blog(LOG_ERROR,
		     "obs-browser: StreamElementsDecryptCefCookiesFile: '%s': %s",
		     path_utf8, sqlite3_errmsg(db));

		sqlite3_close_v2(db);

		return;
	}

	std::vector<std::string> update_statements;

	const char *sql = "select * from cookies where value = '' and encrypted_value is NOT NULL";

	sqlite3_stmt *stmt;

	if (SQLITE_OK == sqlite3_prepare(db, sql, -1, &stmt, nullptr)) {
		std::unordered_map<std::string, int> columnNameMap;

		/* Map column names to their indexes */
		for (int colIndex = 0;; ++colIndex) {
			const char *col_name =
				sqlite3_column_name(stmt, colIndex);
			if (!col_name)
				break;

			columnNameMap[col_name] = colIndex;
		}

		auto getStringValue = [&](const char *colName) -> std::string {
			const int col_index = columnNameMap[colName];
			const unsigned char *val =
				sqlite3_column_text(stmt, col_index);

			if (!!val) {
				const int size =
					sqlite3_column_bytes(stmt, col_index);

				return std::string((const char *)val,
						   (size_t)size);
			}

			return std::string();
		};

		auto getIntValue = [&](const char *colName) -> int {
			const int col_index = columnNameMap[colName];
			return sqlite3_column_int(stmt, col_index);
		};

		if (columnNameMap.count("name") &&
		    columnNameMap.count("encrypted_value") &&
		    columnNameMap.count("host_key") &&
		    columnNameMap.count("path") &&
		    columnNameMap.count("priority")) {
			while (SQLITE_ROW == sqlite3_step(stmt)) {
				const int encrypted_value_index =
					columnNameMap["encrypted_value"];

				const void *encrypted_blob =
					sqlite3_column_blob(
						stmt, encrypted_value_index);
				const int encrypted_size = sqlite3_column_bytes(
					stmt, encrypted_value_index);

				if (!encrypted_size)
					continue;

				std::string host_key =
					getStringValue("host_key");
				std::string path = getStringValue("path");
				std::string name = getStringValue("name");
				int priority = getIntValue("priority");

				DATA_BLOB in;
				in.cbData = encrypted_size;
				in.pbData = (BYTE *)encrypted_blob;

				DATA_BLOB out;

				if (!CryptUnprotectData(&in, NULL, NULL, NULL,
							NULL, 0, &out))
					continue;

				std::string decrypted_value(
					(const char *)out.pbData,
					(size_t)out.cbData);
				::LocalFree(out.pbData);

				char *update_stmt = sqlite3_mprintf(
					"update cookies set value = %Q, encrypted_value = NULL where value = '' and host_key = %Q and name = %Q and priority = %d",
					decrypted_value.c_str(),
					host_key.c_str(), name.c_str(),
					priority);

				update_statements.push_back(update_stmt);

				sqlite3_free(update_stmt);
			}
		} else {
			blog(LOG_ERROR,
			     "obs-browser: StreamElementsDecryptCefCookiesFile: '%s' cookies table is missing required columns",
			     path_utf8);
		}

		sqlite3_finalize(stmt);
	} else {
		blog(LOG_ERROR,
		     "obs-browser: StreamElementsDecryptCefCookiesFile: '%s': %s",
		     path_utf8, sqlite3_errmsg(db));
	}

	int done_count = 0;
	int error_count = 0;

	for (auto item : update_statements) {
		char *zErrMsg;

		if (SQLITE_OK != sqlite3_exec(db, item.c_str(), nullptr,
					      nullptr, &zErrMsg)) {
			blog(LOG_INFO,
			     "obs-browser: StreamElementsDecryptCefCookiesFile: '%s': sqlite3_exec(): %s: %s",
			     path_utf8, zErrMsg, item.c_str());

			sqlite3_free(zErrMsg);

			++error_count;
		} else {
			++done_count;
		}
	}

	sqlite3_close_v2(db);

	blog(LOG_INFO,
	     "obs-browser: StreamElementsDecryptCefCookiesFile: '%s': %d succeeded, %d failed",
	     path_utf8, done_count, error_count);
#endif
}

void StreamElementsDecryptCefCookiesStoragePath(const char* path_utf8) {
	std::string file_path = path_utf8;

	file_path += "/Cookies";

	StreamElementsDecryptCefCookiesFile(file_path.c_str());
}
#endif /* ENABLE_DECRYPT_COOKIES */
