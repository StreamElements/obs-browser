#pragma once

#include "StreamElementsBrowserWidgetManager.hpp"
#include "StreamElementsConfig.hpp"

class StreamElementsGlobalStateManager
{
private:
	StreamElementsGlobalStateManager();
	~StreamElementsGlobalStateManager();

public:
	static StreamElementsGlobalStateManager* GetInstance();

public:
	void Initialize(QMainWindow* obs_main_window);
	void Shutdown();

	StreamElementsBrowserWidgetManager* GetWidgetManager() { return m_widgetManager; }

private:
	bool m_initialized = false;
	StreamElementsBrowserWidgetManager* m_widgetManager = nullptr;

private:
	static StreamElementsGlobalStateManager* s_instance;
};
