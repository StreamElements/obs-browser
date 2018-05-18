#include "StreamElementsWidgetManager.hpp"

#include <cassert>
#include <mutex>

StreamElementsWidgetManager::StreamElementsWidgetManager(QMainWindow* parent) :
	m_parent(parent)
{
	assert(parent);
}

StreamElementsWidgetManager::~StreamElementsWidgetManager()
{
}

void StreamElementsWidgetManager::PushCentralWidget(QWidget* widget)
{
	QWidget* prevWidget = m_parent->takeCentralWidget();

	m_centralWidgetStack.push(prevWidget);

	m_parent->setCentralWidget(widget);
}

QWidget* StreamElementsWidgetManager::PopCentralWidget()
{
	if (m_centralWidgetStack.size()) {
		QWidget* currWidget = m_parent->takeCentralWidget();

		m_parent->setCentralWidget(m_centralWidgetStack.top());

		m_centralWidgetStack.pop();

		return currWidget;
	}

	return nullptr;
}

void StreamElementsWidgetManager::OnObsExit()
{
	// Empty stack
	while (PopCentralWidget()) {
	}
}
