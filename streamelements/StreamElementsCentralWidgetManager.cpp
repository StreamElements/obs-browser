#include "StreamElementsCentralWidgetManager.hpp"

#include <cassert>
#include <mutex>

StreamElementsCentralWidgetManager::StreamElementsCentralWidgetManager(QMainWindow* parent) :
	m_parent(parent)
{
	assert(parent);
}

StreamElementsCentralWidgetManager::~StreamElementsCentralWidgetManager()
{
}

void StreamElementsCentralWidgetManager::Push(QWidget* widget)
{
	QWidget* prevWidget = m_parent->takeCentralWidget();

	m_stack.push(prevWidget);

	m_parent->setCentralWidget(widget);
}

QWidget* StreamElementsCentralWidgetManager::Pop()
{
	if (m_stack.size()) {
		QWidget* currWidget = m_parent->takeCentralWidget();

		m_parent->setCentralWidget(m_stack.top());

		m_stack.pop();

		return currWidget;
	}

	return nullptr;
}
