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
		StreamElementsGlobalStateManager* self;
		QMainWindow* obs_main_window;
	};

	local_context context;

	context.self = this;
	context.obs_main_window = obs_main_window;

	QtExecSync([](void* data) -> void {
		local_context* context = (local_context*)data;

		context->self->m_widgetManager = new StreamElementsBrowserWidgetManager(context->obs_main_window);

		if (StreamElementsConfig::GetInstance()->GetStartupFlags() | StreamElementsConfig::STARTUP_FLAGS_ONBOARDING_MODE) {
			// On-boarding

			context->self->GetWidgetManager()->PushCentralBrowserWidget("http://streamelements.local/onboarding.html", nullptr);
		}
		else {
			// Regular
		}
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
	}, this);

	m_initialized = false;
}