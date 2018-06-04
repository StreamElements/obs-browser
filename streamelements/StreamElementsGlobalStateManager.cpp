#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsUtils.hpp"

#include "base64/base64.hpp"

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

			liveSupport->setStyleSheet(QString("QPushButton { 	border-radius: 15px; 	background-color: #444444; 	color: white; 	padding: 8px; 	padding-left: 40px; 	padding-right: 40px; }  QPushButton:hover { 	background-color: #666666; }  QPushButton:pressed { 	background-color: #eeeeee; 	color: black; }"));

			QDockWidget* controlsDock = (QDockWidget*)context->obs_main_window->findChild<QDockWidget*>("controlsDock");
			QVBoxLayout* buttonsVLayout = (QVBoxLayout*)controlsDock->findChild<QVBoxLayout*>("buttonsVLayout");
			buttonsVLayout->addWidget(liveSupport);

			QObject::connect(liveSupport, &QPushButton::clicked, []()
			{
				QUrl navigate_url = QUrl(obs_module_text("StreamElements.Action.LiveSupport.URL"), QUrl::TolerantMode);
				QDesktopServices::openUrl(navigate_url);
			});
		}


		if (StreamElementsConfig::GetInstance()->GetStartupFlags() & StreamElementsConfig::STARTUP_FLAGS_ONBOARDING_MODE) {
			// On-boarding

			context->self->Reset();
		}
		else {
			// Regular

			context->self->RestoreState();
		}

		QApplication::sendPostedEvents();

		context->self->m_menuManager->Update();
	}, &context);

	QtPostTask([](void* data) {
		// Update visible state
		StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
	}, this);

	m_initialized = true;
	m_persistStateEnabled = true;
}

void StreamElementsGlobalStateManager::Shutdown()
{
	if (!m_initialized) {
		return;
	}

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
	PersistState();
}

void StreamElementsGlobalStateManager::PersistState()
{
	if (!m_persistStateEnabled) {
		return;
	}

	CefRefPtr<CefValue> root = CefValue::Create();
	CefRefPtr<CefDictionaryValue> rootDictionary = CefDictionaryValue::Create();
	root->SetDictionary(rootDictionary);

	CefRefPtr<CefValue> dockingWidgetsState = CefValue::Create();
	CefRefPtr<CefValue> notificationBarState = CefValue::Create();

	GetWidgetManager()->SerializeDockingWidgets(dockingWidgetsState);
	GetWidgetManager()->SerializeNotificationBar(notificationBarState);

	rootDictionary->SetValue("dockingBrowserWidgets", dockingWidgetsState);
	rootDictionary->SetValue("notificationBar", notificationBarState);

	CefString json = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	std::string base64EncodedJSON = base64_encode(json.ToString());

	StreamElementsConfig::GetInstance()->SetStartupState(base64EncodedJSON);
}

void StreamElementsGlobalStateManager::RestoreState()
{
	std::string base64EncodedJSON = StreamElementsConfig::GetInstance()->GetStartupState();

	if (!base64EncodedJSON.size()) {
		return;
	}

	CefString json = base64_decode(base64EncodedJSON);

	if (!json.size()) {
		return;
	}

	CefRefPtr<CefValue> root = CefParseJSON(json, JSON_PARSER_ALLOW_TRAILING_COMMAS);
	CefRefPtr<CefDictionaryValue> rootDictionary = root->GetDictionary();

	if (!rootDictionary.get()) {
		return;
	}

	auto dockingWidgetsState = rootDictionary->GetValue("dockingBrowserWidgets");
	auto notificationBarState = rootDictionary->GetValue("notificationBar");

	GetWidgetManager()->DeserializeDockingWidgets(dockingWidgetsState);
	GetWidgetManager()->DeserializeNotificationBar(notificationBarState);
}

void StreamElementsGlobalStateManager::OnObsExit()
{
	PersistState();

	m_persistStateEnabled = false;
}
