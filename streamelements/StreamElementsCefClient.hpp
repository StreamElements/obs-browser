#pragma once

#include "cef-headers.hpp"

class StreamElementsCefClient :
	public CefClient,
	public CefLifeSpanHandler,
	public CefContextMenuHandler,
	public CefLoadHandler
{
public:
	StreamElementsCefClient(std::string& executeJavaScriptCodeOnLoad) :
		m_executeJavaScriptCodeOnLoad(executeJavaScriptCodeOnLoad)
	{
	}

	inline ~StreamElementsCefClient()
	{
	}

	/* CefClient */
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
	virtual CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

	virtual bool OnProcessMessageReceived(
		CefRefPtr<CefBrowser> browser,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message) override;

	/* CefLifeSpanHandler */
	virtual bool OnBeforePopup(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		const CefString &target_url,
		const CefString &target_frame_name,
		WindowOpenDisposition target_disposition,
		bool user_gesture,
		const CefPopupFeatures &popupFeatures,
		CefWindowInfo &windowInfo,
		CefRefPtr<CefClient> &client,
		CefBrowserSettings &settings,
		bool *no_javascript_access) override
	{
		// Block pop-ups
		return true;
	}

	/* CefContextMenuHandler */
	virtual void OnBeforeContextMenu(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefContextMenuParams> params,
		CefRefPtr<CefMenuModel> model) override
	{
		// Remove all context menu contributions
		model->Clear();
	}

	/* CefLoadHandler */
	virtual void OnLoadEnd(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		int httpStatusCode);

public:
	std::string& GetExecuteJavaScriptCodeOnLoad()
	{
		return m_executeJavaScriptCodeOnLoad;
	}

private:
	std::string m_executeJavaScriptCodeOnLoad;

public:
	IMPLEMENT_REFCOUNTING(StreamElementsCefClient)
};
