#include "StreamElementsMenuManager.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsConfig.hpp"

#include <callback/signal.h>

#include "../cef-headers.hpp"

#include <QObjectList>
#include <QDesktopServices>
#include <QUrl>

#include <algorithm>

StreamElementsMenuManager::StreamElementsMenuManager(QMainWindow *parent)
	: m_mainWindow(parent)
{
	m_auxMenuItems = CefValue::Create();
	m_auxMenuItems->SetNull();

	m_menu = new QMenu("St&reamElements");

	mainWindow()->menuBar()->setFocusPolicy(Qt::NoFocus);

	mainWindow()->menuBar()->addMenu(m_menu);
	m_editMenu = mainWindow()->menuBar()->findChild<QMenu *>(
		"menuBasic_MainMenu_Edit");

	m_nativeEditMenuCopySourceAction = m_editMenu->findChild<QAction *>("actionCopySource");

	m_cefEditMenuActionCopy = new QAction("Copy");
	m_cefEditMenuActionCut = new QAction("Cut");
	m_cefEditMenuActionPaste = new QAction("Paste");

	m_cefEditMenuActionCopy->setShortcut(QKeySequence::Copy);
	m_cefEditMenuActionCut->setShortcut(QKeySequence::Cut);
	m_cefEditMenuActionPaste->setShortcut(QKeySequence::Paste);

	m_cefEditMenuActions.push_back(m_cefEditMenuActionCopy);
	m_cefEditMenuActions.push_back(m_cefEditMenuActionCut);
	m_cefEditMenuActions.push_back(m_cefEditMenuActionPaste);

	m_editMenu->insertAction(m_editMenu->actions().at(0),
				 m_cefEditMenuActionPaste);
	m_editMenu->insertAction(m_editMenu->actions().at(0),
				 m_cefEditMenuActionCut);
	m_editMenu->insertAction(m_editMenu->actions().at(0),
				 m_cefEditMenuActionCopy);

	QObject::connect(
		qApp->clipboard(), &QClipboard::dataChanged, this,
		&StreamElementsMenuManager::HandleClipboardDataChanged);

	QObject::connect(m_cefEditMenuActionCopy, &QAction::triggered, this,
			 &StreamElementsMenuManager::HandleCefCopy);
	QObject::connect(m_cefEditMenuActionCut, &QAction::triggered, this,
			 &StreamElementsMenuManager::HandleCefCut);
	QObject::connect(m_cefEditMenuActionPaste, &QAction::triggered, this,
			 &StreamElementsMenuManager::HandleCefPaste);

	UpdateEditMenuInternal();

	LoadConfig();
}

StreamElementsMenuManager::~StreamElementsMenuManager()
{
	QObject::disconnect(m_cefEditMenuActionCopy, &QAction::triggered, this,
			    &StreamElementsMenuManager::HandleCefCopy);
	QObject::disconnect(m_cefEditMenuActionCut, &QAction::triggered, this,
			    &StreamElementsMenuManager::HandleCefCut);
	QObject::disconnect(m_cefEditMenuActionPaste, &QAction::triggered, this,
			    &StreamElementsMenuManager::HandleCefPaste);

	QObject::disconnect(
		qApp->clipboard(), &QClipboard::dataChanged, this,
		&StreamElementsMenuManager::HandleClipboardDataChanged);

	for (auto i : m_cefEditMenuActions) {
		m_editMenu->removeAction(i);
	}
	m_cefEditMenuActions.clear();

	//mainWindow()->menuBar()->removeAction((QAction *)m_menu->menuAction());
	m_menu->menuAction()->setVisible(false);
	m_menu = nullptr;
}

void StreamElementsMenuManager::SetFocusedBrowserWidget(
	StreamElementsBrowserWidget *widget)
{
	// Must be called on QT app thread

	if (m_focusedBrowserWidget == widget)
		return;

	if (!widget && !!m_focusedBrowserWidget) {
		QObject::disconnect(
			m_focusedBrowserWidget,
			&StreamElementsBrowserWidget::
				browserFocusedDOMNodeEditableChanged,
			this,
			&StreamElementsMenuManager::
				HandleFocusedWidgetDOMNodeEditableChanged);
	}

	m_focusedBrowserWidget = widget;

	if (!!m_focusedBrowserWidget) {
		QObject::connect(m_focusedBrowserWidget,
				 &StreamElementsBrowserWidget::
					 browserFocusedDOMNodeEditableChanged,
				 this,
				 &StreamElementsMenuManager::
					 HandleFocusedWidgetDOMNodeEditableChanged);
	}

	UpdateEditMenuInternal();
}

void StreamElementsMenuManager::HandleCefCopy()
{
	// Must be called on QT app thread

	if (!m_focusedBrowserWidget)
		return;

	m_focusedBrowserWidget->BrowserCopy();
}

void StreamElementsMenuManager::HandleCefCut()
{
	// Must be called on QT app thread

	if (!m_focusedBrowserWidget)
		return;

	m_focusedBrowserWidget->BrowserCut();
}

void StreamElementsMenuManager::HandleCefPaste()
{
	// Must be called on QT app thread

	if (!m_focusedBrowserWidget)
		return;

	m_focusedBrowserWidget->BrowserPaste();
}

void StreamElementsMenuManager::HandleFocusedWidgetDOMNodeEditableChanged(
	bool isEditable)
{
	// Must be called on QT app thread

	UpdateEditMenuInternal();
}

void StreamElementsMenuManager::HandleClipboardDataChanged()
{
	// Must be called on QT app thread

	UpdateEditMenuInternal();
}

void StreamElementsMenuManager::Update()
{
	SYNC_ACCESS();

	UpdateInternal();
}

void StreamElementsMenuManager::UpdateEditMenuInternal()
{
	// Must be called on QT app thread

	const bool editMenuVisible = !!m_focusedBrowserWidget;

	for (auto i : m_cefEditMenuActions) {
		i->setVisible(editMenuVisible);
	}

	m_nativeEditMenuCopySourceAction->setVisible(!editMenuVisible);

	if (!editMenuVisible)
		return;

	const bool isEditable =
		m_focusedBrowserWidget->isBrowserFocusedDOMNodeEditable();

	m_cefEditMenuActionCopy->setEnabled(isEditable);
	m_cefEditMenuActionCut->setEnabled(isEditable);

	auto mimeData = qApp->clipboard()->mimeData();

	const bool hasTextToPaste = mimeData && mimeData->hasText();

	m_cefEditMenuActionPaste->setEnabled(isEditable && hasTextToPaste);
}

void StreamElementsMenuManager::UpdateInternal()
{
	SYNC_ACCESS();

	if (!m_menu)
		return;

	m_menu->clear();

	auto createURL = [this](QString title, QString url) -> QAction * {
		QAction *menu_action = new QAction(title);

		menu_action->connect(
			menu_action, &QAction::triggered, [this, url] {
				QUrl navigate_url =
					QUrl(url, QUrl::TolerantMode);
				QDesktopServices::openUrl(navigate_url);
			});

		return menu_action;
	};

	if (m_showBuiltInMenuItems) {
		QMenu *setupMenu = new QMenu(obs_module_text(
			"StreamElements.Action.SetupContainer"));

		m_menu->addMenu(setupMenu);

		QAction *onboarding_action = new QAction(obs_module_text(
			"StreamElements.Action.ForceOnboarding"));
		setupMenu->addAction(onboarding_action);
		onboarding_action->connect(onboarding_action, &QAction::triggered, [this] {
			StreamElementsGlobalStateManager::GetInstance()
				->Reset(false,
					StreamElementsGlobalStateManager::
						OnBoarding);
		});

		setupMenu->addAction(createURL(
			obs_module_text("StreamElements.Action.Overlays"),
			obs_module_text("StreamElements.Action.Overlays.URL")));

		// Docks
		{
			QMenu *docksMenu = new QMenu(obs_module_text(
				"StreamElements.Action.DocksContainer"));

			DeserializeDocksMenu(*docksMenu);

			m_menu->addMenu(docksMenu);
		}

		QAction *import_action = new QAction(
			obs_module_text("StreamElements.Action.Import"));
		m_menu->addAction(import_action);
		import_action->connect(import_action, &QAction::triggered, [this] {
			StreamElementsGlobalStateManager::GetInstance()
				->Reset(false,
					StreamElementsGlobalStateManager::
						Import);
		});

		m_menu->addSeparator();
	}

	DeserializeMenu(m_auxMenuItems, *m_menu);

	m_menu->addSeparator();

	QAction *check_for_updates_action = new QAction(
		obs_module_text("StreamElements.Action.CheckForUpdates"));
	m_menu->addAction(check_for_updates_action);
	check_for_updates_action->connect(
		check_for_updates_action, &QAction::triggered, [this] {
			calldata_t *cd = calldata_create();
			calldata_set_bool(cd, "allow_downgrade", false);
			calldata_set_bool(cd, "force_install", false);
			calldata_set_bool(cd, "allow_use_last_response", false);

			signal_handler_signal(
				obs_get_signal_handler(),
				"streamelements_request_check_for_updates",
				cd);

			calldata_free(cd);
		});

	QMenu *helpMenu = new QMenu(
		obs_module_text("StreamElements.Action.HelpContainer"));

	m_menu->addMenu(helpMenu);

	QAction *report_issue = new QAction(
		obs_module_text("StreamElements.Action.ReportIssue"));
	helpMenu->addAction(report_issue);
	report_issue->connect(report_issue, &QAction::triggered, [this] {
		StreamElementsGlobalStateManager::GetInstance()->ReportIssue();
	});

	helpMenu->addAction(createURL(
		obs_module_text("StreamElements.Action.LiveSupport.MenuItem"),
		obs_module_text("StreamElements.Action.LiveSupport.URL")));

	m_menu->addSeparator();

	{
		bool isLoggedIn =
			StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN ==
			(StreamElementsConfig::GetInstance()->GetStartupFlags() &
			 StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN);

		auto reset_action_handler = [this] {
			StreamElementsGlobalStateManager::
				GetInstance()
					->Reset();
		};

		if (isLoggedIn) {
			m_menu->setStyleSheet(
				"QMenu::item:default { color: #F53656; }");

			QAction *logout_action = new QAction(obs_module_text(
				"StreamElements.Action.ResetStateSignOut"));
			logout_action->setObjectName("signOutAction");

			m_menu->addAction(logout_action);
			logout_action->connect(logout_action,
					       &QAction::triggered,
					       reset_action_handler);
			m_menu->setDefaultAction(logout_action);
		} else {
			QAction *login_action = new QAction(obs_module_text(
				"StreamElements.Action.ResetStateSignIn"));
			m_menu->addAction(login_action);
			login_action->connect(login_action,
					       &QAction::triggered,
					       reset_action_handler);
		}
	}
}

bool StreamElementsMenuManager::DeserializeAuxiliaryMenuItems(
	CefRefPtr<CefValue> input)
{
	SYNC_ACCESS();

	QMenu menu;
	bool result = DeserializeMenu(input, menu);

	if (result) {
		m_auxMenuItems = input->Copy();
	}

	Update();

	SaveConfig();

	return result;
}

void StreamElementsMenuManager::Reset()
{
	SYNC_ACCESS();

	m_auxMenuItems->SetNull();
	m_showBuiltInMenuItems = true;

	Update();

	SaveConfig();
}

void StreamElementsMenuManager::SerializeAuxiliaryMenuItems(
	CefRefPtr<CefValue> &output)
{
	output = m_auxMenuItems->Copy();
}

void StreamElementsMenuManager::SaveConfig()
{
	SYNC_ACCESS();

	StreamElementsConfig::GetInstance()->SetAuxMenuItemsConfig(
		CefWriteJSON(m_auxMenuItems, JSON_WRITER_DEFAULT).ToString());

	StreamElementsConfig::GetInstance()->SetShowBuiltInMenuItems(
		m_showBuiltInMenuItems);
}

void StreamElementsMenuManager::LoadConfig()
{
	SYNC_ACCESS();

	m_showBuiltInMenuItems =
		StreamElementsConfig::GetInstance()->GetShowBuiltInMenuItems();

	CefRefPtr<CefValue> val = CefParseJSON(
		StreamElementsConfig::GetInstance()->GetAuxMenuItemsConfig(),
		JSON_PARSER_ALLOW_TRAILING_COMMAS);

	if (!val.get() || val->GetType() != VTYPE_LIST)
		return;

	DeserializeAuxiliaryMenuItems(val);
}

void StreamElementsMenuManager::SetShowBuiltInMenuItems(bool show)
{
	m_showBuiltInMenuItems = show;

	Update();
}

bool StreamElementsMenuManager::GetShowBuiltInMenuItems()
{
	return m_showBuiltInMenuItems;
}
