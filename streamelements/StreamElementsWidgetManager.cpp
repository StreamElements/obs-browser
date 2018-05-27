#include "StreamElementsWidgetManager.hpp"
#include "StreamElementsUtils.hpp"

#include <cassert>
#include <mutex>

#include <QApplication>

StreamElementsWidgetManager::StreamElementsWidgetManager(QMainWindow* parent) :
	m_parent(parent)
{
	assert(parent);
}

StreamElementsWidgetManager::~StreamElementsWidgetManager()
{
}

//
// The QApplication::processEvents() and QtPostTask() black magic
// below is a very unpleasant hack to ensure that the central
// widget remains the same size as the previously visible central
// widget.
//
// This is done by first measuring the size() of the current
// central widget, then removing the current central widget and
// replacing it with a new one, setting the new widget MINIMUM
// size to the previously visible central widget width & height
// and submitting a task to the Qt message queue for later
// execution to reset the new central widget minimum size on the
// following iteration of the Qt message queue processing loop.
//
void StreamElementsWidgetManager::PushCentralWidget(QWidget* widget)
{
	QApplication::processEvents();
	QSize prevSize = mainWindow()->centralWidget()->size();

	widget->setMinimumSize(prevSize);

	QWidget* prevWidget = m_parent->takeCentralWidget();

	m_centralWidgetStack.push(prevWidget);

	m_parent->setCentralWidget(widget);

	QtPostTask([](void* data) {
		QWidget* widget = static_cast<QWidget*>(data);

		widget->setMinimumSize(0, 0);
	}, mainWindow()->centralWidget());
}

QWidget* StreamElementsWidgetManager::PopCentralWidget()
{
	if (m_centralWidgetStack.size()) {
		QApplication::processEvents();
		QSize currSize = mainWindow()->centralWidget()->size();

		QWidget* currWidget = m_parent->takeCentralWidget();

		m_parent->setCentralWidget(m_centralWidgetStack.top());

		m_centralWidgetStack.pop();

		mainWindow()->centralWidget()->setMinimumSize(currSize);

		QtPostTask([](void* data) {
			QWidget* widget = static_cast<QWidget*>(data);

			widget->setMinimumSize(0, 0);
		}, mainWindow()->centralWidget());

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

bool StreamElementsWidgetManager::AddDockWidget(
	const char* const id,
	const char* const title,
	QWidget* const widget,
	const Qt::DockWidgetArea area,
	const Qt::DockWidgetAreas allowedAreas,
	const QDockWidget::DockWidgetFeatures features)
{
	assert(id);
	assert(title);
	assert(widget);

	if (m_dockWidgets.count(id)) {
		return false;
	}

	QDockWidget* dock = new QDockWidget(title, m_parent);

	dock->setAllowedAreas(allowedAreas);
	dock->setFeatures(features);
	dock->setWidget(widget);

	m_parent->addDockWidget(area, dock);

	m_dockWidgets[id] = dock;
	m_dockWidgetAreas[id] = area;

	QObject::connect(dock, &QDockWidget::dockLocationChanged, [id, dock, this](Qt::DockWidgetArea area) {
		if (!m_dockWidgets.count(id)) {
			return;
		}

		m_dockWidgetAreas[id] = area;
	});

	return true;
}

bool StreamElementsWidgetManager::RemoveDockWidget(const char* const id)
{
	assert(id);

	if (!m_dockWidgets.count(id)) {
		return false;
	}

	QDockWidget* dock = m_dockWidgets[id];
	m_dockWidgets.erase(id);
	m_dockWidgetAreas.erase(id);

	m_parent->removeDockWidget(dock);

	delete dock;

	return true;
}

void StreamElementsWidgetManager::GetDockWidgetIdentifiers(std::vector<std::string>& result)
{
	for (auto imap : m_dockWidgets) {
		result.emplace_back(imap.first);
	}
}

QDockWidget* StreamElementsWidgetManager::GetDockWidget(const char* const id)
{
	assert(id);

	if (!m_dockWidgets.count(id)) {
		return nullptr;
	}

	return m_dockWidgets[id];
}

StreamElementsWidgetManager::DockWidgetInfo* StreamElementsWidgetManager::GetDockWidgetInfo(const char* const id)
{
	assert(id);

	QDockWidget* dockWidget = GetDockWidget(id);

	if (!dockWidget) {
		return nullptr;
	}

	StreamElementsWidgetManager::DockWidgetInfo* result = new StreamElementsWidgetManager::DockWidgetInfo();

	result->m_widget = dockWidget->widget();

	result->m_id = id;
	result->m_title = dockWidget->windowTitle().toStdString();
	result->m_visible = dockWidget->isVisible();

	if (dockWidget->isFloating()) {
		result->m_dockingArea = "floating";
	} else {
		switch (m_dockWidgetAreas[id])
		{
		case Qt::LeftDockWidgetArea:
			result->m_dockingArea = "left";
			break;
		case Qt::RightDockWidgetArea:
			result->m_dockingArea = "right";
			break;
		case Qt::TopDockWidgetArea:
			result->m_dockingArea = "top";
			break;
		case Qt::BottomDockWidgetArea:
			result->m_dockingArea = "bottom";
			break;
		//case Qt::NoDockWidgetArea:
		default:
			result->m_dockingArea = "floating";
			break;
		}
	}

	return result;
}
