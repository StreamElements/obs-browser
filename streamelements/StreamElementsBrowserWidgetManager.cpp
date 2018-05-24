#include "StreamElementsBrowserWidgetManager.hpp"

#include "cef-headers.hpp"

#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

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

void StreamElementsBrowserWidgetManager::SerializeDockingWidgets(std::string& output)
{
	CefRefPtr<CefValue> root = CefValue::Create();

	CefRefPtr<CefDictionaryValue> rootDictionary = CefDictionaryValue::Create();
	root->SetDictionary(rootDictionary);

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
		}

		rootDictionary->SetValue(id, widgetValue);
	}

	// Convert data to JSON
	CefString jsonString =
		CefWriteJSON(root, JSON_WRITER_DEFAULT);

	output = jsonString.ToString();
}

void StreamElementsBrowserWidgetManager::DeserializeDockingWidgets(std::string& input)
{
	CefRefPtr<CefValue> root =
		CefParseJSON(CefString(input), JSON_PARSER_ALLOW_TRAILING_COMMAS);

	if (!root.get()) {
		return;
	}

	CefRefPtr<CefDictionaryValue> rootDictionary =
		root->GetDictionary();

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
				int requestWidth = 200;
				int requestHeight = 200;

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
					requestHeight = rec.height() / 2;
				}
				else if (dockingAreaString == "right") {
					dockingArea = Qt::RightDockWidgetArea;
					sizePolicy.setVerticalPolicy(QSizePolicy::Expanding);
					requestHeight = rec.height() / 2;
				}
				else if (dockingAreaString == "top") {
					dockingArea = Qt::TopDockWidgetArea;
					sizePolicy.setHorizontalPolicy(QSizePolicy::Expanding);
					requestWidth = rec.width() / 2;
				}
				else if (dockingAreaString == "bottom") {
					dockingArea = Qt::BottomDockWidgetArea;
					sizePolicy.setHorizontalPolicy(QSizePolicy::Expanding);
					requestWidth = rec.width() / 2;
				}

				if (widgetDictionary->HasKey("minWidth")) {
					requestWidth = widgetDictionary->GetInt("minWidth");
					sizePolicy.setHorizontalPolicy(QSizePolicy::Expanding);
				}

				if (widgetDictionary->HasKey("preferWidth")) {
					requestWidth = widgetDictionary->GetInt("preferWidth");
					sizePolicy.setHorizontalPolicy(QSizePolicy::Preferred);
				}

				if (widgetDictionary->HasKey("maxWidth")) {
					requestWidth = widgetDictionary->GetInt("maxWidth");
					sizePolicy.setHorizontalPolicy(QSizePolicy::Maximum);
				}

				if (widgetDictionary->HasKey("minHeight")) {
					requestHeight = widgetDictionary->GetInt("minHeight");
					sizePolicy.setVerticalPolicy(QSizePolicy::Expanding);
				}

				if (widgetDictionary->HasKey("preferHeight")) {
					requestHeight = widgetDictionary->GetInt("preferHeight");
					sizePolicy.setVerticalPolicy(QSizePolicy::Preferred);
				}

				if (widgetDictionary->HasKey("maxHeight")) {
					requestHeight = widgetDictionary->GetInt("maxHeight");
					sizePolicy.setVerticalPolicy(QSizePolicy::Maximum);
				}

				if (AddDockBrowserWidget(
					id.ToString().c_str(),
					title.c_str(),
					url.c_str(),
					executeJavaScriptOnLoad.c_str(),
					dockingArea)) {
					QWidget* widget = GetDockWidget(id.ToString().c_str());

					widget->setMinimumWidth(requestWidth);
					widget->setMinimumHeight(requestHeight);

					widget->setSizePolicy(sizePolicy);

					switch (sizePolicy.verticalPolicy())
					{
					case QSizePolicy::Minimum:
					case QSizePolicy::MinimumExpanding:
					case QSizePolicy::Expanding:
						widget->setMinimumHeight(requestHeight);
						break;
					case QSizePolicy::Preferred:
						widget->setMinimumHeight(requestHeight);
						widget->setMaximumHeight(requestHeight);
						break;
					case QSizePolicy::Maximum:
						widget->setMaximumHeight(requestHeight);
						break;
					}

					switch (sizePolicy.horizontalPolicy())
					{
					case QSizePolicy::Minimum:
					case QSizePolicy::MinimumExpanding:
						widget->setMinimumWidth(requestWidth);
						break;
					case QSizePolicy::Preferred:
						widget->setMinimumWidth(requestWidth);
						widget->setMaximumWidth(requestWidth);
						break;
					case QSizePolicy::Maximum:
						widget->setMaximumWidth(requestWidth);
						break;
					}
				}
			}
		}
	}
}
