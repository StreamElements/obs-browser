#pragma once

#include <QMainWindow>

#include "cef-headers.hpp"

#include <mutex>

class StreamElementsSceneCollectionsManager
{
public:
	StreamElementsSceneCollectionsManager(QMainWindow* parent);
	virtual ~StreamElementsSceneCollectionsManager();

	bool IsAvailable();

	void SerializeSceneCollections(CefRefPtr<CefValue>& output);
	void SerializeCurrentSceneCollection(CefRefPtr<CefValue>& output);
	bool DeserializeSceneCollection(CefRefPtr<CefValue>& input);
	bool SetCurrentSceneCollection(std::string id);

protected:
	QMainWindow* mainWindow() { return m_parent; }

private:
	char** (*m_obs_frontend_get_scene_collections)(void);
	char* (*m_obs_frontend_get_current_scene_collection)(void);
	void (*m_obs_frontend_set_current_scene_collection)(const char *collection);
	bool (*m_obs_frontend_add_scene_collection)(const char *name);

private:
	void*	m_obs_frontend_module = nullptr;

private:
	std::recursive_mutex m_mutex;

	QMainWindow* m_parent;
};
