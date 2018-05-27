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

#include <QtWidgets>

class StreamElementsBrowserWidget:
	public QWidget

{
	Q_OBJECT

private:
	std::string m_url;
	std::string m_executeJavaScriptCodeOnLoad;

	QSize m_sizeHint;

public:
	StreamElementsBrowserWidget(QWidget* parent, const char* const url, const char* const executeJavaScriptCodeOnLoad);
	~StreamElementsBrowserWidget();

	void setSizeHint(QSize& size)
	{
		m_sizeHint = size;
	}

	void setSizeHint(const int w, const int h)
	{
		m_sizeHint = QSize(w, h);
	}

	/*
	virtual QSize sizeHint() const override
	{
		QSize size = QWidget::sizeHint();

		if (size.isValid() && !size.isNull() && !size.isEmpty())
			return size;
		else
			return m_sizeHint;
	}

	virtual QSize minimumSizeHint() const override
	{
		QSize size = QWidget::minimumSizeHint();

		if (size.isValid() && !size.isNull() && !size.isEmpty())
			return size;
		else
			return QSize(m_sizeHint.width(), m_sizeHint.height() / 2);
	}
	*/

public:
	std::string GetCurrentUrl();
	std::string GetExecuteJavaScriptCodeOnLoad();

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

	virtual void resizeEvent(QResizeEvent* event) override
	{
		QWidget::resizeEvent(event);

		UpdateBrowserSize();
	}

private:
	void UpdateBrowserSize()
	{
		if (!!m_cef_browser.get()) {
			HWND hWnd = m_cef_browser->GetHost()->GetWindowHandle();

			SetWindowPos(hWnd, HWND_TOP, 0, 0, width(), height(), SWP_DRAWFRAME | SWP_SHOWWINDOW);
		}
	}

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

