#pragma once

#include <cef-headers.hpp>
#include <obs.h>
#include <obs-module.h>

#include <memory>
#include <iostream>
#include <mutex>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <map>

#include <functional>

#include <util/threading.h>
#include <util/platform.h>
#include <curl/curl.h>

#define SYNC_ACCESS()                                                    \
	static std::recursive_mutex __sync_access_mutex;                 \
	std::lock_guard<std::recursive_mutex> __sync_access_mutex_guard( \
		__sync_access_mutex);

#include <QTimer>
#include <QApplication>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QWidget>

template<typename... Args> std::string FormatString(const char *format, ...)
{
	int size = 512;
	char *buffer = 0;
	buffer = new char[size];
	va_list vl;
	va_start(vl, format);
	int nsize = vsnprintf(buffer, size, format, vl);
	if (size <= nsize) { //fail delete buffer and try again
		delete[] buffer;
		buffer = 0;
		buffer = new char[nsize + 1]; //+1 for /0
		nsize = vsnprintf(buffer, size, format, vl);
	}
	std::string ret(buffer);
	va_end(vl);
	delete[] buffer;
	return ret;
}

void QtPostTask(void (*func)(void *), void *const data);
void QtPostTask(std::function<void()> task);
void QtExecSync(void (*func)(void *), void *const data);
void QtExecSync(std::function<void()> task);
std::string DockWidgetAreaToString(const Qt::DockWidgetArea area);
std::string GetCommandLineOptionValue(const std::string key);
std::string LoadResourceString(std::string path);

/* ========================================================= */

void SerializeSystemTimes(CefRefPtr<CefValue> &output);
void SerializeSystemMemoryUsage(CefRefPtr<CefValue> &output);
void SerializeSystemHardwareProperties(CefRefPtr<CefValue> &output);

/* ========================================================= */

void SerializeAvailableInputSourceTypes(CefRefPtr<CefValue> &output);

/* ========================================================= */

std::string GetCurrentThemeName();
std::string SerializeAppStyleSheet();
std::string GetAppStyleSheetSelectorContent(std::string selector);

/* ========================================================= */

std::string GetCefVersionString();
std::string GetCefPlatformApiHash();
std::string GetCefUniversalApiHash();
std::string GetStreamElementsPluginVersionString();
std::string GetStreamElementsApiVersionString();

/* ========================================================= */

void SetGlobalCURLOptions(CURL *curl, const char *url);

typedef std::function<bool(void *data, size_t datalen, void *userdata,
			   char *error_msg, int http_code)>
	http_client_callback_t;
typedef std::function<void(char *data, void *userdata, char *error_msg,
			   int http_code)>
	http_client_string_callback_t;
typedef std::multimap<std::string, std::string> http_client_request_headers_t;

bool HttpGet(const char *url, http_client_request_headers_t request_headers,
	     http_client_callback_t callback, void *userdata);

bool HttpPost(const char *url, http_client_request_headers_t request_headers,
	      void *buffer, size_t buffer_len, http_client_callback_t callback,
	      void *userdata);

bool HttpGetString(const char *url,
		   http_client_request_headers_t request_headers,
		   http_client_string_callback_t callback, void *userdata);

bool HttpPostString(const char *url,
		    http_client_request_headers_t request_headers,
		    const char *postData,
		    http_client_string_callback_t callback, void *userdata);

/* ========================================================= */

std::string CreateGloballyUniqueIdString();
std::string GetComputerSystemUniqueId();

/* ========================================================= */

struct streamelements_env_update_request {
public:
	std::string product;
	std::string key;
	std::string value;
};

typedef std::vector<streamelements_env_update_request>
	streamelements_env_update_requests;

std::string ReadProductEnvironmentConfigurationString(const char *key);
bool WriteProductEnvironmentConfigurationString(const char *key,
						const char *value);
bool WriteProductEnvironmentConfigurationStrings(
	streamelements_env_update_requests requests);
bool WriteEnvironmentConfigStrings(streamelements_env_update_requests requests);
bool WriteEnvironmentConfigString(const char *regValueName,
				  const char *regValue,
				  const char *productName);

/* ========================================================= */

bool ParseQueryString(std::string input,
		      std::map<std::string, std::string> &result);
std::string CreateSHA256Digest(std::string &input);
std::string CreateSessionMessageSignature(std::string &message);
bool VerifySessionMessageSignature(std::string &message,
				   std::string &signature);
std::string CreateSessionSignedAbsolutePathURL(std::wstring path);
bool VerifySessionSignedAbsolutePathURL(std::string url, std::string &path);

/* ========================================================= */

bool IsAlwaysOnTop(QWidget *window);
void SetAlwaysOnTop(QWidget *window, bool enable);

/* ========================================================= */

class RecursiveNestingLevelCounter {
public:
	RecursiveNestingLevelCounter(long *counter) : m_counter(counter)
	{
		m_level = os_atomic_inc_long(m_counter);
	}

	~RecursiveNestingLevelCounter() { os_atomic_dec_long(m_counter); }

	long level() { return m_level; }

private:
	long *m_counter;
	long m_level;
};

#define PREVENT_RECURSIVE_REENTRY()                                   \
	static long __recursive_nesting_level = 0L;                   \
	RecursiveNestingLevelCounter __recursive_nesting_level_guard( \
		&__recursive_nesting_level);                          \
	if (__recursive_nesting_level_guard.level() > 1)              \
		return;

double GetObsGlobalFramesPerSecond();

/* ========================================================= */

void AdviseHostUserInterfaceStateChanged();

/* ========================================================= */

bool ParseStreamElementsOverlayURL(std::string url, std::string &overlayId,
				   std::string &accountId);

std::string GetStreamElementsOverlayEditorURL(std::string overlayId,
					      std::string accountId);

/* ========================================================= */

#if ENABLE_DECRYPT_COOKIES
void StreamElementsDecryptCefCookiesFile(const char *path_utf8);
void StreamElementsDecryptCefCookiesStoragePath(const char *path_utf8);
#endif /* ENABLE_DECRYPT_COOKIES */

/* ========================================================= */

std::string GetIdFromPointer(const void *ptr);
const void *GetPointerFromId(const char *id);

/* ========================================================= */

bool GetTemporaryFilePath(std::string prefixString, std::string &result);
std::string GetUniqueFileNameFromPath(std::string path, size_t maxLength);
std::string GetFolderPathFromFilePath(std::string filePath);

/* ========================================================= */

bool ReadListOfObsSceneCollections(std::map<std::string, std::string> &output);
bool ReadListOfObsProfiles(std::map<std::string, std::string> &output);
