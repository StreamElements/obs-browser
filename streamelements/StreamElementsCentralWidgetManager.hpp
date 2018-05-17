#pragma once

#include <QWidget>
#include <QMainWindow>
#include <vector>
#include <stack>

class StreamElementsCentralWidgetManager
{
public:
	StreamElementsCentralWidgetManager(QMainWindow* parent);
	~StreamElementsCentralWidgetManager();

	void Push(QWidget* widget);
	QWidget* Pop();

private:
	std::stack<QWidget*> m_stack;
	QMainWindow* m_parent;
};
