#pragma once

#include <QWidget>
#include <QDockWidget>
#include <QMainWindow>
#include <stack>
#include <map>

#include "StreamElementsObsAppMonitor.hpp"

class StreamElementsWidgetManager :
	public StreamElementsObsAppMonitor
{
public:
	class DockWidgetInfo
	{
	public:
		std::string m_id;
		std::string m_title;
		bool m_visible;
		std::string m_dockingArea;

	private:
		QWidget* m_widget;

		friend class StreamElementsWidgetManager;

	public:
		QWidget* GetWidget() { return m_widget; }
	};

public:
	StreamElementsWidgetManager(QMainWindow* parent);
	~StreamElementsWidgetManager();

	/* central widget */

	void PushCentralWidget(QWidget* widget);
	QWidget* PopCentralWidget();

	/* dockable widgets */

	bool AddDockWidget(
		const char* const id,
		const char* const title,
		QWidget* const widget,
		const Qt::DockWidgetArea area,
		const Qt::DockWidgetAreas allowedAreas = Qt::AllDockWidgetAreas,
		const QDockWidget::DockWidgetFeatures features = QDockWidget::AllDockWidgetFeatures);

	virtual bool RemoveDockWidget(const char* const id);

	void GetDockWidgetIdentifiers(std::vector<std::string>& result);

	QDockWidget* GetDockWidget(const char* const id);

	DockWidgetInfo* GetDockWidgetInfo(const char* const id);

	virtual void SerializeDockingWidgets(std::string& output) = 0;
	virtual void DeserializeDockingWidgets(std::string& input) = 0;

private:
	QMainWindow * m_parent;

	std::stack<QWidget*> m_centralWidgetStack;

	std::map<std::string, QDockWidget*> m_dockWidgets;
	std::map<std::string, Qt::DockWidgetArea> m_dockWidgetAreas;

protected:
	virtual void OnObsExit();
};
