#pragma once

#include "StreamElementsBrowserWidgetManager.hpp"
#include "StreamElementsMenuManager.hpp"
#include "StreamElementsConfig.hpp"
#include "StreamElementsObsAppMonitor.hpp"

class StreamElementsGlobalStateManager :
	public StreamElementsObsAppMonitor
{
private:
	StreamElementsGlobalStateManager();
	virtual ~StreamElementsGlobalStateManager();

public:
	static StreamElementsGlobalStateManager* GetInstance();

public:
	void Initialize(QMainWindow* obs_main_window);
	void Shutdown();

	void Reset();

	void PersistState();
	void RestoreState();

	StreamElementsBrowserWidgetManager* GetWidgetManager() { return m_widgetManager; }
	StreamElementsMenuManager* GetMenuManager() { return m_menuManager; }

protected:
	virtual void OnObsExit() override;

private:
	bool m_persistStateEnabled = false;
	bool m_initialized = false;
	StreamElementsBrowserWidgetManager* m_widgetManager = nullptr;
	StreamElementsMenuManager* m_menuManager = nullptr;

private:
	static StreamElementsGlobalStateManager* s_instance;
};
