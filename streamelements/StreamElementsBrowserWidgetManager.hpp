#pragma once

#include "StreamElementsWidgetManager.hpp"
#include "StreamElementsBrowserWidget.hpp"

class StreamElementsBrowserWidgetManager :
	protected StreamElementsWidgetManager
{
public:
	class DockBrowserWidgetInfo :
		public DockWidgetInfo
	{
	public:
		DockBrowserWidgetInfo()
		{
		}

		DockBrowserWidgetInfo(DockWidgetInfo& other): DockWidgetInfo(other)
		{
		}

		DockBrowserWidgetInfo(DockBrowserWidgetInfo& other) : DockWidgetInfo(other)
		{
			m_url = other.m_url;
			m_executeJavaScriptOnLoad = other.m_executeJavaScriptOnLoad;
		}

	public:
		std::string m_url;
		std::string m_executeJavaScriptOnLoad;
	};

public:
	StreamElementsBrowserWidgetManager(QMainWindow* parent) :
		StreamElementsWidgetManager(parent)
	{
	}

	virtual ~StreamElementsBrowserWidgetManager() { }

	/* central widget */

	void PushCentralBrowserWidget(
		const char* const url,
		const char* const executeJavaScriptCodeOnLoad);

	bool PopCentralBrowserWidget();

	/* dockable widgets */

	bool AddDockBrowserWidget(
		const char* const id,
		const char* const title,
		const char* const url,
		const char* const executeJavaScriptCodeOnLoad,
		const Qt::DockWidgetArea area,
		const Qt::DockWidgetAreas allowedAreas = Qt::AllDockWidgetAreas,
		const QDockWidget::DockWidgetFeatures features = QDockWidget::AllDockWidgetFeatures);

	virtual bool RemoveDockWidget(const char* const id) override;

	void GetDockBrowserWidgetIdentifiers(std::vector<std::string>& result);

	DockBrowserWidgetInfo* GetDockBrowserWidgetInfo(const char* const id);

	QDockWidget* GetDockWidget(const char* const id) {
		return StreamElementsWidgetManager::GetDockWidget(id);
	}

	virtual void SerializeDockingWidgets(std::string& output) override;
	virtual void DeserializeDockingWidgets(std::string& input) override;

private:
	std::map<std::string, StreamElementsBrowserWidget*> m_browserWidgets;
};
