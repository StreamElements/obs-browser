#pragma once

#include <QWidget>

#include <util/platform.h>
#include <util/threading.h>
#include <include/cef_base.h>
#include <include/cef_version.h>
#include <include/cef_app.h>
#include <include/cef_task.h>
#include <include/base/cef_bind.h>
#include <include/wrapper/cef_closure_task.h>
#include <include/base/cef_lock.h>

#include <pthread.h>
#include <functional>

#include "../browser-client.hpp"

#include "StreamElementsAsyncTaskQueue.hpp"

class StreamElementsBrowserWidget:
	public QWidget,
	public CefClient,
	public CefLifeSpanHandler

{
	Q_OBJECT

private:
	// Default browser handle when browser is not yet initialized
	static const int BROWSER_HANDLE_NONE = -1;

public:
	StreamElementsBrowserWidget(QWidget* parent);
	~StreamElementsBrowserWidget();

public:
	// CefClient methods.
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE {
		return this;
	}

	// CefLifeSpanHandler methods.
	void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE;
	bool DoClose(CefRefPtr<CefBrowser> browser) OVERRIDE;
	void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE;

private:
	///
	// Browser initialization
	//
	// Create browser or navigate back to home page (obs-browser-wcui-browser-dialog.html)
	//
	void CefUIThreadInitBrowser();
	void CefUIThreadExecute(std::function<void()> func, bool async);

private:
	StreamElementsAsyncTaskQueue m_task_queue;
	cef_window_handle_t m_window_handle;
	CefRefPtr<CefBrowser> m_cef_browser;

protected:
	virtual void showEvent(QShowEvent* showEvent) override
	{
		QWidget::showEvent(showEvent);

		/*
		if (m_cef_browser.get() == NULL) {
			m_window_handle = (cef_window_handle_t)winId();

			CefPostTask(TID_UI, base::Bind(&StreamElementsBrowserWidget::CefUIThreadInitBrowser, this));
		} else {
			m_cef_browser->GetHost()->WasHidden(false);
		}
		*/
	}

	virtual void hideEvent(QHideEvent *hideEvent) override
	{
		/*
		if (m_cef_browser.get() != NULL) {
			m_cef_browser->GetHost()->WasHidden(true);
			m_cef_browser->GetHost()->CloseBrowser(true);

			//CefUIThreadExecute([this]() {
			//	m_cef_browser->GetHost()->WasHidden(true);
			//}, false);

			m_cef_browser = NULL;
		}
		*/

		QWidget::hideEvent(hideEvent);
	}

private:







public:
	IMPLEMENT_REFCOUNTING(StreamElementsBrowserWidget)
};

