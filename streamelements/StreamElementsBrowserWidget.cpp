#include "StreamElementsBrowserWidget.hpp"
#include "StreamElementsCefClient.hpp"
#include <functional>

static class BrowserTask : public CefTask {
public:
	std::function<void()> task;

	inline BrowserTask(std::function<void()> task_) : task(task_) {}
	virtual void Execute() override { task(); }

	IMPLEMENT_REFCOUNTING(BrowserTask);
};

static bool QueueCEFTask(std::function<void()> task)
{
	return CefPostTask(TID_UI, CefRefPtr<BrowserTask>(new BrowserTask(task)));
}

/* ========================================================================= */

StreamElementsBrowserWidget::StreamElementsBrowserWidget(QWidget* parent):
	QWidget(parent),
	m_window_handle(0)
{
	// Create native window
	setAttribute(Qt::WA_NativeWindow);

	// This influences docking widget width/height
	//setMinimumWidth(400);
	//setMinimumHeight(200);

	QSizePolicy policy;
	policy.setHorizontalPolicy(QSizePolicy::Expanding);
	policy.setVerticalPolicy(QSizePolicy::Expanding);
	setSizePolicy(policy);
	//, QSizePolicy::ExpandFlag
}


StreamElementsBrowserWidget::~StreamElementsBrowserWidget()
{
	DestroyBrowser();
}

#ifdef ___NO_SUCH_THING___
void StreamElementsBrowserWidget::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
	// Must be executed on the UI thread.
	//CEF_REQUIRE_UI_THREAD();
	
	//if (!m_cef_browser.get()) {
		// Keep a reference to the main browser.
		//m_cef_browser = browser;
		//m_BrowserId = browser->GetIdentifier();
	//}

	// Keep track of how many browsers currently exist.
	//m_BrowserCount++;
}

bool StreamElementsBrowserWidget::DoClose(CefRefPtr<CefBrowser> browser)
{
	// Must be executed on the UI thread.
	//CEF_REQUIRE_UI_THREAD();

	// Closing the main window requires special handling. See the DoClose()
	// documentation in the CEF header for a detailed description of this
	// process.
	//if (m_BrowserId == browser->GetIdentifier()) {
	//	// Set a flag to indicate that the window close should be allowed.
	//	m_bIsClosing = true;
	//}

	// Allow the close. For windowed browsers this will result in the OS close
	// event being sent.

	m_cef_browser = NULL;

	return false;
}

void StreamElementsBrowserWidget::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
	// Must be executed on the UI thread.
	//CEF_REQUIRE_UI_THREAD();

	//if (m_BrowserId == browser->GetIdentifier()) {
	//	// Free the browser pointer so that the browser can be destroyed.
	//	m_Browser = NULL;
	//}

	//if (--m_BrowserCount == 0) {
	//	// All browser windows have closed. Quit the application message loop.
	//	CefQuitMessageLoop();
	//}
}
#endif

void StreamElementsBrowserWidget::InitBrowserAsync()
{
	// Make sure InitBrowserAsyncInternal() runs in Qt UI thread
	QMetaObject::invokeMethod(this, "InitBrowserAsyncInternal");
}

void StreamElementsBrowserWidget::InitBrowserAsyncInternal()
{
	if (!!m_cef_browser.get())
		return;

	m_window_handle = (cef_window_handle_t)winId();

	CefUIThreadExecute([this]() {
		StreamElementsBrowserWidget* self = this;

		CefString url = "http://www.google.com/";

		// Client area rectangle
		RECT clientRect;

		clientRect.left = 0;
		clientRect.top = 0;
		clientRect.right = self->width();
		clientRect.bottom = self->height();

		// CEF window attributes
		CefWindowInfo windowInfo;
		windowInfo.width = clientRect.right - clientRect.left;
		windowInfo.height = clientRect.bottom - clientRect.top;
		windowInfo.windowless_rendering_enabled = false;
		windowInfo.SetAsChild(self->m_window_handle, clientRect);
		//windowInfo.SetAsPopup(0, CefString("Window Name"));

		CefBrowserSettings cefBrowserSettings;

		cefBrowserSettings.Reset();
		cefBrowserSettings.javascript_close_windows = STATE_DISABLED;
		cefBrowserSettings.local_storage = STATE_DISABLED;

		m_cef_browser =
			CefBrowserHost::CreateBrowserSync(
				windowInfo,
				new StreamElementsCefClient(),
				url,
				cefBrowserSettings,
				nullptr);

		UpdateBrowserSize();
	}, true);
}

void StreamElementsBrowserWidget::CefUIThreadExecute(std::function<void()> func, bool async)
{
	if (!async) {
		os_event_t *finishedEvent;
		os_event_init(&finishedEvent, OS_EVENT_TYPE_AUTO);
		bool success = QueueCEFTask([&]() {
			func();

			os_event_signal(finishedEvent);
		});
		if (success)
			os_event_wait(finishedEvent);
		os_event_destroy(finishedEvent);
	}
	else {
		QueueCEFTask([this, func]() {
			func();
		});
	}
}
