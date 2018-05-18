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

	bool RemoveDockWidget(const char* const id);

	std::vector<std::string>& GetDockWidgetIdentifiers();

	QDockWidget* GetDockWidget(const char* const id);

private:
	QMainWindow * m_parent;

	std::stack<QWidget*> m_centralWidgetStack;

	std::map<std::string, QDockWidget*> m_dockWidgets;

protected:
	virtual void OnObsExit();
};
