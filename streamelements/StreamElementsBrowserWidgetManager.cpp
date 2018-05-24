#include "StreamElementsBrowserWidgetManager.hpp"

void StreamElementsBrowserWidgetManager::PushCentralBrowserWidget(
	const char* const url,
	const char* const executeJavaScriptCodeOnLoad)
{
	PushCentralWidget(new StreamElementsBrowserWidget(nullptr, url, executeJavaScriptCodeOnLoad));
}

bool StreamElementsBrowserWidgetManager::PopCentralBrowserWidget()
{
	QWidget* result = PopCentralWidget();

	if (result) {
		delete result;

		return true;
	} else {
		return false;
	}
}

bool StreamElementsBrowserWidgetManager::AddDockBrowserWidget(
	const char* const id,
	const char* const title,
	const char* const url,
	const char* const executeJavaScriptCodeOnLoad,
	const Qt::DockWidgetArea area,
	const Qt::DockWidgetAreas allowedAreas,
	const QDockWidget::DockWidgetFeatures features)
{
	StreamElementsBrowserWidget* widget = new StreamElementsBrowserWidget(nullptr, url, executeJavaScriptCodeOnLoad);

	if (AddDockWidget(id, title, widget, area, allowedAreas, features)) {
		m_browserWidgets[id] = widget;
	} else {
		return false;
	}
}

bool StreamElementsBrowserWidgetManager::RemoveDockWidget(const char* const id)
{
	if (m_browserWidgets.count(id)) {
		m_browserWidgets.erase(id);
	}
	return StreamElementsWidgetManager::RemoveDockWidget(id);
}

std::vector<std::string>& StreamElementsBrowserWidgetManager::GetDockBrowserWidgetIdentifiers()
{
	return GetDockWidgetIdentifiers();
}

StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo* StreamElementsBrowserWidgetManager::GetDockBrowserWidgetInfo(const char* const id)
{
	StreamElementsBrowserWidgetManager::DockWidgetInfo* baseInfo = GetDockWidgetInfo(id);

	if (!baseInfo) {
		return nullptr;
	}

	StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo* result =
		new StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo(*baseInfo);

	delete baseInfo;

	result->m_url = m_browserWidgets[id]->GetCurrentUrl();

	// TODO: Implement
	result->m_executeJavaScriptOnLoad = m_browserWidgets[id]->GetExecuteJavaScriptCodeOnLoad();

	return result;
}
