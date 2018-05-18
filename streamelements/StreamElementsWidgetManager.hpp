#pragma once

#include <QWidget>
#include <QMainWindow>
#include <vector>
#include <stack>

#include "StreamElementsObsAppMonitor.hpp"

class StreamElementsWidgetManager :
	public StreamElementsObsAppMonitor
{
public:
	StreamElementsWidgetManager(QMainWindow* parent);
	~StreamElementsWidgetManager();

	void PushCentralWidget(QWidget* widget);
	QWidget* PopCentralWidget();

private:
	std::stack<QWidget*> m_centralWidgetStack;
	QMainWindow* m_parent;

protected:
	virtual void OnObsExit();
};
