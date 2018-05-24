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

	std::vector<std::string>& GetDockWidgetIdentifiers();

	QDockWidget* GetDockWidget(const char* const id);

	DockWidgetInfo* GetDockWidgetInfo(const char* const id);

private:
	QMainWindow * m_parent;

	std::stack<QWidget*> m_centralWidgetStack;

	std::map<std::string, QDockWidget*> m_dockWidgets;
	std::map<std::string, Qt::DockWidgetArea> m_dockWidgetAreas;

protected:
	virtual void OnObsExit();
};
