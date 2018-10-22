#include "StreamElementsSceneCollectionsManager.hpp"

#include <obs.h>
#include "util/platform.h"

#include <thread>

#include <QApplication>

StreamElementsSceneCollectionsManager::StreamElementsSceneCollectionsManager(QMainWindow* parent):
	m_parent(parent)
{
	m_obs_frontend_module = os_dlopen("obs-frontend-api");

	if (m_obs_frontend_module) {
		m_obs_frontend_get_scene_collections =
			(decltype(m_obs_frontend_get_scene_collections))
				os_dlsym(m_obs_frontend_module,
					"obs_frontend_get_scene_collections");

		m_obs_frontend_get_current_scene_collection =
			(decltype(m_obs_frontend_get_current_scene_collection))
			os_dlsym(m_obs_frontend_module,
				"obs_frontend_get_current_scene_collection");

		m_obs_frontend_set_current_scene_collection =
			(decltype(m_obs_frontend_set_current_scene_collection))
			os_dlsym(m_obs_frontend_module,
				"obs_frontend_set_current_scene_collection");

		m_obs_frontend_add_scene_collection =
			(decltype(m_obs_frontend_add_scene_collection))
			os_dlsym(m_obs_frontend_module,
				"obs_frontend_add_scene_collection");
	}
}

StreamElementsSceneCollectionsManager::~StreamElementsSceneCollectionsManager()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_obs_frontend_module) {
		os_dlclose(m_obs_frontend_module);
		m_obs_frontend_module = nullptr;
	}
}

bool StreamElementsSceneCollectionsManager::IsAvailable()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_obs_frontend_module) {
		return false;
	}

	if (!m_obs_frontend_get_scene_collections) {
		return false;
	}

	if (!m_obs_frontend_get_current_scene_collection) {
		return false;
	}

	if (!m_obs_frontend_set_current_scene_collection) {
		return false;
	}

	if (!m_obs_frontend_add_scene_collection) {
		return false;
	}

	return true;
}

void StreamElementsSceneCollectionsManager::SerializeSceneCollections(CefRefPtr<CefValue>& output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!IsAvailable()) {
		return;
	}

	CefRefPtr<CefListValue> list = CefListValue::Create();

	std::string current = m_obs_frontend_get_current_scene_collection();

	char** names = m_obs_frontend_get_scene_collections();
	char** name = names;

	while (name && *name) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("id", *name);
		d->SetString("name", *name);

		list->SetDictionary(list->GetSize(), d);

		++name;
	}

	bfree(names);

	output->SetList(list);
}

void StreamElementsSceneCollectionsManager::SerializeCurrentSceneCollection(CefRefPtr<CefValue>& output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!IsAvailable()) {
		return;
	}

	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	char* name = m_obs_frontend_get_current_scene_collection();

	d->SetString("id", name);
	d->SetString("name", name);

	bfree(name);

	output->SetDictionary(d);
}

bool StreamElementsSceneCollectionsManager::DeserializeSceneCollection(CefRefPtr<CefValue>& input)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!IsAvailable()) {
		return false;
	}

	if (input->GetType() != VTYPE_DICTIONARY) {
		return false;
	}

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d.get()) {
		return false;
	}

	if (!d->HasKey("name")) {
		return false;
	}

	std::string name = d->GetString("name").ToString();

	if (!name.size()) {
		return false;
	}

	bool result = m_obs_frontend_add_scene_collection(name.c_str());

	return result;
}

bool StreamElementsSceneCollectionsManager::SetCurrentSceneCollection(std::string id)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!IsAvailable()) {
		return false;
	}

	if (!id.size()) {
		return false;
	}

	bool isValid = false;

	char** names = m_obs_frontend_get_scene_collections();
	char** name = names;

	while (name && *name && !isValid) {
		if (id == *name) {
			isValid = true;
			break;
		}

		++name;
	}

	bfree(names);

	if (!isValid) {
		return false;
	}

	m_obs_frontend_set_current_scene_collection(id.c_str());

	//
	// This is an unpleasant hack to make sure current scene collection
	// transition completes before we release control.
	//

	bool done = false;

	while (!done) {
		QApplication::processEvents();

		char* name = m_obs_frontend_get_current_scene_collection();

		if (name == id) {
			done = true;
		}

		bfree(name);
	}

	return true;
}
