#pragma once

#include <QWidget>
#include <QHideEvent>

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
	public QWidget

{
	Q_OBJECT

public:
	StreamElementsBrowserWidget(QWidget* parent);
	~StreamElementsBrowserWidget();

private:
	///
	// Browser initialization
	//
	// Create browser or navigate back to home page (obs-browser-wcui-browser-dialog.html)
	//
	void InitBrowserAsync();
	void CefUIThreadExecute(std::function<void()> func, bool async);

private slots:
	void InitBrowserAsyncInternal();

private:
	StreamElementsAsyncTaskQueue m_task_queue;
	cef_window_handle_t m_window_handle;
	CefRefPtr<CefBrowser> m_cef_browser;

protected:
	virtual void showEvent(QShowEvent* showEvent) override
	{
		QWidget::showEvent(showEvent);

		InitBrowserAsync();
	}

	virtual void hideEvent(QHideEvent *hideEvent) override
	{
		QWidget::hideEvent(hideEvent);
	}

private:
	void DestroyBrowser()
	{
		if (m_cef_browser.get() != NULL) {
			m_cef_browser->GetHost()->CloseBrowser(true);
			m_cef_browser = NULL;
		}
	}






public:
//	IMPLEMENT_REFCOUNTING(StreamElementsBrowserWidget)
};

