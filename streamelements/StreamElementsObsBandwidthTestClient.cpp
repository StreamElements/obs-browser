#include "StreamElementsObsBandwidthTestClient.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

StreamElementsObsBandwidthTestClient::StreamElementsObsBandwidthTestClient()
{
	obs_frontend_add_event_callback(obs_frontend_event_handler, this);
}

StreamElementsObsBandwidthTestClient::~StreamElementsObsBandwidthTestClient()
{
}

void StreamElementsObsBandwidthTestClient::obs_frontend_event_handler(enum obs_frontend_event event, void *data)
{
	StreamElementsObsBandwidthTestClient* self = (StreamElementsObsBandwidthTestClient*)data;

	if (event == OBS_FRONTEND_EVENT_EXIT)
	{
		self->CancelAll();

		obs_frontend_remove_event_callback((obs_frontend_event_cb)data, data);
	}
}
