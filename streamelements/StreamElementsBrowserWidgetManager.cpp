#include "StreamElementsBrowserWidgetManager.hpp"

#include "cef-headers.hpp"

#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

#include "StreamElementsUtils.hpp"

void StreamElementsBrowserWidgetManager::PushCentralBrowserWidget(
	const char* const url,
	const char* const executeJavaScriptCodeOnLoad)
{
	PushCentralWidget(new StreamElementsBrowserWidget(nullptr, url, executeJavaScriptCodeOnLoad));
}

bool StreamElementsBrowserWidgetManager::PopCentralBrowserWidget()
{
	QWidget* result = PopCentralWidget();

	if (result) {
		delete result;

		return true;
	} else {
		return false;
	}
}

bool StreamElementsBrowserWidgetManager::AddDockBrowserWidget(
	const char* const id,
	const char* const title,
	const char* const url,
	const char* const executeJavaScriptCodeOnLoad,
	const Qt::DockWidgetArea area,
	const Qt::DockWidgetAreas allowedAreas,
	const QDockWidget::DockWidgetFeatures features)
{
	StreamElementsBrowserWidget* widget = new StreamElementsBrowserWidget(nullptr, url, executeJavaScriptCodeOnLoad);

	if (AddDockWidget(id, title, widget, area, allowedAreas, features)) {
		m_browserWidgets[id] = widget;
	} else {
		return false;
	}
}

bool StreamElementsBrowserWidgetManager::RemoveDockWidget(const char* const id)
{
	if (m_browserWidgets.count(id)) {
		m_browserWidgets.erase(id);
	}
	return StreamElementsWidgetManager::RemoveDockWidget(id);
}

void StreamElementsBrowserWidgetManager::GetDockBrowserWidgetIdentifiers(std::vector<std::string>& result)
{
	return GetDockWidgetIdentifiers(result);
}

StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo* StreamElementsBrowserWidgetManager::GetDockBrowserWidgetInfo(const char* const id)
{
	StreamElementsBrowserWidgetManager::DockWidgetInfo* baseInfo = GetDockWidgetInfo(id);

	if (!baseInfo) {
		return nullptr;
	}

	StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo* result =
		new StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo(*baseInfo);

	delete baseInfo;

	result->m_url = m_browserWidgets[id]->GetCurrentUrl();

	result->m_executeJavaScriptOnLoad = m_browserWidgets[id]->GetExecuteJavaScriptCodeOnLoad();

	return result;
}

void StreamElementsBrowserWidgetManager::SerializeDockingWidgets(CefRefPtr<CefValue>& output)
{
	CefRefPtr<CefDictionaryValue> rootDictionary = CefDictionaryValue::Create();
	output->SetDictionary(rootDictionary);

	std::vector<std::string> widgetIdentifiers;

	GetDockBrowserWidgetIdentifiers(widgetIdentifiers);

	for (auto id : widgetIdentifiers) {
		CefRefPtr<CefValue> widgetValue = CefValue::Create();
		CefRefPtr<CefDictionaryValue> widgetDictionary = CefDictionaryValue::Create();
		widgetValue->SetDictionary(widgetDictionary);

		StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo* info =
			GetDockBrowserWidgetInfo(id.c_str());

		if (info) {
			widgetDictionary->SetString("id", id);
			widgetDictionary->SetString("url", info->m_url);
			widgetDictionary->SetString("dockingArea", info->m_dockingArea);
			widgetDictionary->SetString("title", info->m_title);
			widgetDictionary->SetString("executeJavaScriptOnLoad", info->m_executeJavaScriptOnLoad);
			widgetDictionary->SetBool("visible", info->m_visible);

			QWidget* widget = GetDockWidget(id.c_str());

			QSize minSize = widget->minimumSize();
			QSize size = widget->size();

			widgetDictionary->SetInt("minWidth", minSize.width());
			widgetDictionary->SetInt("minHeight", minSize.height());

			widgetDictionary->SetInt("width", size.width());
			widgetDictionary->SetInt("height", size.height());

			widgetDictionary->SetInt("left", widget->pos().x());
			widgetDictionary->SetInt("top", widget->pos().y());
		}

		rootDictionary->SetValue(id, widgetValue);
	}
}

void StreamElementsBrowserWidgetManager::DeserializeDockingWidgets(CefRefPtr<CefValue>& input)
{
	if (!input.get()) {
		return;
	}

	CefRefPtr<CefDictionaryValue> rootDictionary =
		input->GetDictionary();

	CefDictionaryValue::KeyList rootDictionaryKeys;

	if (!rootDictionary->GetKeys(rootDictionaryKeys)) {
		return;
	}

	for (auto id : rootDictionaryKeys) {
		CefRefPtr<CefValue> widgetValue = rootDictionary->GetValue(id);

		CefRefPtr<CefDictionaryValue> widgetDictionary = widgetValue->GetDictionary();

		if (widgetDictionary.get()) {
			if (widgetDictionary->HasKey("title") && widgetDictionary->HasKey("url")) {
				std::string title;
				std::string url;
				std::string dockingAreaString = "floating";
				std::string executeJavaScriptOnLoad;
				bool visible;
				QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
				int requestWidth = 100;
				int requestHeight = 100;
				int minWidth = 0;
				int minHeight = 0;
				int left = -1;
				int top = -1;

				QRect rec = QApplication::desktop()->screenGeometry();

				title = widgetDictionary->GetString("title");
				url = widgetDictionary->GetString("url");

				if (widgetDictionary->HasKey("dockingArea")) {
					dockingAreaString = widgetDictionary->GetString("dockingArea");
				}

				if (widgetDictionary->HasKey("executeJavaScriptOnLoad")) {
					executeJavaScriptOnLoad = widgetDictionary->GetString("executeJavaScriptOnLoad");
				}

				if (widgetDictionary->HasKey("visible")) {
					visible = widgetDictionary->GetBool("visible");
				}

				Qt::DockWidgetArea dockingArea = Qt::NoDockWidgetArea;

				if (dockingAreaString == "left") {
					dockingArea = Qt::LeftDockWidgetArea;
					sizePolicy.setVerticalPolicy(QSizePolicy::Expanding);
				}
				else if (dockingAreaString == "right") {
					dockingArea = Qt::RightDockWidgetArea;
					sizePolicy.setVerticalPolicy(QSizePolicy::Expanding);
				}
				else if (dockingAreaString == "top") {
					dockingArea = Qt::TopDockWidgetArea;
					sizePolicy.setHorizontalPolicy(QSizePolicy::Expanding);
				}
				else if (dockingAreaString == "bottom") {
					dockingArea = Qt::BottomDockWidgetArea;
					sizePolicy.setHorizontalPolicy(QSizePolicy::Expanding);
				}

				if (widgetDictionary->HasKey("minWidth")) {
					minWidth = widgetDictionary->GetInt("minWidth");
				}

				if (widgetDictionary->HasKey("width")) {
					requestWidth = widgetDictionary->GetInt("width");
				}

				if (widgetDictionary->HasKey("minHeight")) {
					minHeight = widgetDictionary->GetInt("minHeight");
				}

				if (widgetDictionary->HasKey("height")) {
					requestHeight = widgetDictionary->GetInt("height");
				}

				if (widgetDictionary->HasKey("left")) {
					left = widgetDictionary->GetInt("left");
				}

				if (widgetDictionary->HasKey("top")) {
					top = widgetDictionary->GetInt("top");
				}

				if (AddDockBrowserWidget(
					id.ToString().c_str(),
					title.c_str(),
					url.c_str(),
					executeJavaScriptOnLoad.c_str(),
					dockingArea)) {
					QWidget* widget = GetDockWidget(id.ToString().c_str());

					//QSize savedMaxSize = widget->maximumSize();

					widget->setSizePolicy(sizePolicy);
					widget->setMinimumSize(requestWidth, requestHeight);
					//widget->setMaximumSize(requestWidth, requestHeight);

					QApplication::sendPostedEvents();

					widget->setMinimumSize(minWidth, minHeight);
					//widget->setMaximumSize(savedMaxSize);

					if (left >= 0 || top >= 0) {
						widget->move(left, top);
					}

					QApplication::sendPostedEvents();
				}
			}
		}
	}
}
