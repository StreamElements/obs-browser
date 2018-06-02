#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsUtils.hpp"

#include <util/threading.h>

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
		os_event_t* event;
		StreamElementsGlobalStateManager* self;
		QMainWindow* obs_main_window;
	};

	local_context context;

	os_event_init(&context.event, OS_EVENT_TYPE_AUTO);
	context.self = this;
	context.obs_main_window = obs_main_window;

	QtPostTask([](void* data) -> void {
		local_context* context = (local_context*)data;

		context->self->m_widgetManager = new StreamElementsBrowserWidgetManager(context->obs_main_window);

		if (StreamElementsConfig::GetInstance()->GetStartupFlags() | StreamElementsConfig::STARTUP_FLAGS_ONBOARDING_MODE) {
			// On-boarding

			context->self->GetWidgetManager()->PushCentralBrowserWidget("http://streamelements.local/onboarding.html", nullptr);
		}
		else {
			// Regular
		}

		os_event_signal(context->event);
	}, &context);

	QApplication::sendPostedEvents();

	os_event_wait(context.event);
	os_event_destroy(context.event);

	m_initialized = true;
}

void StreamElementsGlobalStateManager::Shutdown()
{
	if (!m_initialized)
		return;

	struct local_context {
		os_event_t* event;
		StreamElementsGlobalStateManager* self;
	};

	local_context context;

	os_event_init(&context.event, OS_EVENT_TYPE_AUTO);
	context.self = this;

	QtPostTask([](void* data) -> void {
		local_context* context = (local_context*)data;

		delete context->self->m_widgetManager;

		os_event_signal(context->event);
	}, &context);

	QApplication::sendPostedEvents();

	os_event_wait(context.event);
	os_event_destroy(context.event);

	m_initialized = false;
}
