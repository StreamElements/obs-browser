#include "StreamElementsMenuManager.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include "../cef-headers.hpp"

#include <QObjectList>
#include <QDesktopServices>
#include <QUrl>

StreamElementsMenuManager::StreamElementsMenuManager(QMainWindow* parent):
	m_mainWindow(parent)
{
	m_menu = new QMenu("StreamElements");

	mainWindow()->menuBar()->addMenu(m_menu);
}

StreamElementsMenuManager::~StreamElementsMenuManager()
{
	m_menu->menuAction()->setVisible(false);
	m_menu = nullptr;
}

void StreamElementsMenuManager::Update()
{
	if (!m_menu) return;

	m_menu->clear();

	auto addURL = [this](QString title, QString url) {
		QAction* menu_action = new QAction(title);
		m_menu->addAction(menu_action);

		menu_action->connect(
			menu_action,
			&QAction::triggered,
			[this, url]
			{
				QUrl navigate_url = QUrl(url, QUrl::TolerantMode);
				QDesktopServices::openUrl(navigate_url);
			});
	};

	QAction* logout_action = new QAction(obs_module_text("StreamElements.Menu.ResetState"));
	m_menu->addAction(logout_action);
	logout_action->connect(
		logout_action,
		&QAction::triggered,
		[this]
		{
			StreamElementsGlobalStateManager::GetInstance()->Reset();
		});


	m_menu->addSeparator();
	addURL(obs_module_text("StreamElements.Menu.Bot"), obs_module_text("StreamElements.Menu.Bot.URL"));
	addURL(obs_module_text("StreamElements.Menu.Tipping"), obs_module_text("StreamElements.Menu.Tipping.URL"));
	addURL(obs_module_text("StreamElements.Menu.Overlays"), obs_module_text("StreamElements.Menu.Overlays.URL"));
	addURL(obs_module_text("StreamElements.Menu.GroundControl"), obs_module_text("StreamElements.Menu.GroundControl.URL"));
	m_menu->addSeparator();
	m_menu->addSeparator();
	addURL(obs_module_text("StreamElements.Menu.Import"), obs_module_text("StreamElements.Menu.Import.URL"));
	m_menu->addSeparator();
	addURL(obs_module_text("StreamElements.Menu.LiveSupport"), obs_module_text("StreamElements.Menu.LiveSupport.URL"));
}
