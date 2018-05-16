#pragma once

#include "StreamElementsBandwidthTestClient.hpp"

class StreamElementsObsBandwidthTestClient :
	public StreamElementsBandwidthTestClient
{
public:
	StreamElementsObsBandwidthTestClient();
	virtual ~StreamElementsObsBandwidthTestClient();

private:
	static void obs_frontend_event_handler(enum obs_frontend_event event, void *data);
};
