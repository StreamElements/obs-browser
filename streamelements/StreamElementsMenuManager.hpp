#pragma once

#include "cef-headers.hpp"

#include "StreamElementsApiMessageHandler.hpp"
#include "StreamElementsBrowserWidget.hpp"

#include <QObject>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>

#include <string>
#include <vector>

class StreamElementsMenuManager :
	public QObject
{
	Q_OBJECT;

private:
	enum aux_menu_item_type_t { Command, Separator, Container };

public:
	StreamElementsMenuManager(QMainWindow* parent);
	virtual ~StreamElementsMenuManager();

public:
	void Update();

	bool DeserializeAuxiliaryMenuItems(CefRefPtr<CefValue> input);
	void SerializeAuxiliaryMenuItems(CefRefPtr<CefValue>& output);

	void Reset();

	void SetShowBuiltInMenuItems(bool show);
	bool GetShowBuiltInMenuItems();

	void SetFocusedBrowserWidget(StreamElementsBrowserWidget *widget);

protected:
	QMainWindow* mainWindow() { return m_mainWindow; }

	void SaveConfig();
	void LoadConfig();

private:
	void UpdateInternal();
	void UpdateEditMenuInternal();

	void HandleFocusedWidgetDOMNodeEditableChanged(bool isEditable);
	void HandleClipboardDataChanged();

	void HandleCefCopy();
	void HandleCefCut();
	void HandleCefPaste();
	void HandleCefSelectAll();

	void AddMenuActions();
	void RemoveMenuActions();

private:
	QMainWindow* m_mainWindow;
	QMenu *m_menu;

	QMenu *m_editMenu;
	std::vector<QAction *> m_cefEditMenuActions;
	QAction *m_cefEditMenuActionCopy = nullptr;
	QAction *m_cefEditMenuActionCut = nullptr;
	QAction *m_cefEditMenuActionPaste = nullptr;
	QAction *m_cefEditMenuActionSelectAll = nullptr;
	QAction *m_nativeEditMenuCopySourceAction = nullptr;

	StreamElementsBrowserWidget *m_focusedBrowserWidget = nullptr;

	CefRefPtr<CefValue> m_auxMenuItems = CefValue::Create();
	bool m_showBuiltInMenuItems = true;
};
