#pragma once

#include "StreamElementsBrowserWidgetManager.hpp"
#include "StreamElementsMenuManager.hpp"
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

	void Reset();

	StreamElementsBrowserWidgetManager* GetWidgetManager() { return m_widgetManager; }
	StreamElementsMenuManager* GetMenuManager() { return m_menuManager; }

private:
	bool m_initialized = false;
	StreamElementsBrowserWidgetManager* m_widgetManager = nullptr;
	StreamElementsMenuManager* m_menuManager = nullptr;

private:
	static StreamElementsGlobalStateManager* s_instance;
};
