#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsUtils.hpp"

#include <util/threading.h>

#include <QPushButton>

StreamElementsGlobalStateManager* StreamElementsGlobalStateManager::s_instance = nullptr;

StreamElementsGlobalStateManager::StreamElementsGlobalStateManager()
{
}

StreamElementsGlobalStateManager::~StreamElementsGlobalStateManager()
{
	Shutdown();
}

StreamElementsGlobalStateManager* StreamElementsGlobalStateManager::GetInstance()
{
	if (!s_instance) {
		s_instance = new StreamElementsGlobalStateManager();
	}

	return s_instance;
}

void StreamElementsGlobalStateManager::Initialize(QMainWindow* obs_main_window)
{
	struct local_context {
		StreamElementsGlobalStateManager* self;
		QMainWindow* obs_main_window;
	};

	local_context context;

	context.self = this;
	context.obs_main_window = obs_main_window;

	QtExecSync([](void* data) -> void {
		local_context* context = (local_context*)data;

		context->self->m_widgetManager = new StreamElementsBrowserWidgetManager(context->obs_main_window);
		context->self->m_menuManager = new StreamElementsMenuManager(context->obs_main_window);

		{
			// Set up "Live Support" button
			QPushButton* liveSupport = new QPushButton(
				QIcon(QPixmap(QString(":/images/icon.png"))),
				obs_module_text("StreamElements.Action.LiveSupport"));

			QDockWidget* controlsDock = (QDockWidget*)context->obs_main_window->findChild<QDockWidget*>("controlsDock");
			QVBoxLayout* buttonsVLayout = (QVBoxLayout*)controlsDock->findChild<QVBoxLayout*>("buttonsVLayout");
			buttonsVLayout->addWidget(liveSupport);

			QObject::connect(liveSupport, &QPushButton::clicked, []()
			{
				QUrl navigate_url = QUrl(obs_module_text("StreamElements.Action.LiveSupport.URL"), QUrl::TolerantMode);
				QDesktopServices::openUrl(navigate_url);
			});
		}


		if (StreamElementsConfig::GetInstance()->GetStartupFlags() | StreamElementsConfig::STARTUP_FLAGS_ONBOARDING_MODE) {
			// On-boarding

			context->self->Reset();
		}
		else {
			// Regular
		}

		context->self->m_menuManager->Update();
	}, &context);

	m_initialized = true;
}

void StreamElementsGlobalStateManager::Shutdown()
{
	if (!m_initialized)
		return;

	QtExecSync([](void* data) -> void {
		StreamElementsGlobalStateManager* self = (StreamElementsGlobalStateManager*)data;

		delete self->m_widgetManager;
		delete self->m_menuManager;
	}, this);

	m_initialized = false;
}

void StreamElementsGlobalStateManager::Reset()
{
	CefCookieManager::GetGlobalManager(NULL)->DeleteCookies(
		CefString(""), // URL
		CefString(""), // Cookie name
		nullptr);      // On-complete callback

	GetWidgetManager()->HideNotificationBar();
	GetWidgetManager()->RemoveAllDockWidgets();
	GetWidgetManager()->DestroyCurrentCentralBrowserWidget();
	GetWidgetManager()->PushCentralBrowserWidget(obs_module_text("StreamElementsOnBoardingURL"), nullptr);

	StreamElementsConfig::GetInstance()->SetStartupFlags(StreamElementsConfig::STARTUP_FLAGS_ONBOARDING_MODE);

	GetMenuManager()->Update();
}
