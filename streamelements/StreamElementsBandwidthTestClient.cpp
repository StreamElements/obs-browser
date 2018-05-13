#include "StreamElementsBandwidthTestClient.hpp"
#include <util/platform.h>
#include <util/threading.h>

StreamElementsBandwidthTestClient::StreamElementsBandwidthTestClient()
{
}


StreamElementsBandwidthTestClient::~StreamElementsBandwidthTestClient()
{
}

uint64_t StreamElementsBandwidthTestClient::TestServerBitsPerSecond(
	const char* serverUrl,
	const char* streamKey,
	const int maxBitrateBitsPerSecond,
	const char* bindToIP,
	const int durationSeconds)
{
	uint64_t resultBitsPerSecond = 0L;

	enum state_enum {
		Starting = 0,
		Running = 1,
		Stopped = 2
	};

	struct local_context
	{
	public:
		os_event_t* event_state_changed;
		state_enum state = Starting;

		local_context()
		{
			os_event_init(&event_state_changed, OS_EVENT_TYPE_AUTO);
		}

		~local_context()
		{
			os_event_destroy(event_state_changed);
		}

		void wait_state_changed()
		{
			os_event_wait(event_state_changed);
		}

		void wait_state_changed(unsigned long milliseconds)
		{
			os_event_timedwait(event_state_changed, milliseconds);
		}

		void signal_state_changed()
		{
			os_event_signal(event_state_changed);
		}
	};

	local_context context;

	obs_encoder_t* vencoder = obs_video_encoder_create("obs_x264", "test_x264", nullptr, nullptr);
	obs_encoder_t* aencoder = obs_audio_encoder_create("ffmpeg_aac", "test_aac", nullptr, 0, nullptr);
	obs_service_t* service = obs_service_create("rtmp_custom", "test_service", nullptr, nullptr);

	obs_data_t* service_settings = obs_data_create();

	// Configure video encoder
	obs_data_t* vencoder_settings = obs_data_create();

	obs_data_set_int(vencoder_settings, "bitrate", (int)(maxBitrateBitsPerSecond / 1000L));
	obs_data_set_string(vencoder_settings, "rate_control", "CBR");
	obs_data_set_string(vencoder_settings, "preset", "veryfast");
	obs_data_set_int(vencoder_settings, "keyint_sec", 2);

	obs_encoder_update(vencoder, vencoder_settings);

	obs_encoder_set_video(vencoder, obs_get_video());

	// Configure audio encoder
	obs_data_t* aencoder_settings = obs_data_create();

	obs_data_set_int(aencoder_settings, "bitrate", 128);

	obs_encoder_update(aencoder, aencoder_settings);

	obs_encoder_set_audio(aencoder, obs_get_audio());

	// Configure service

	obs_data_set_string(service_settings, "service", "rtmp_custom");
	obs_data_set_string(service_settings, "server", serverUrl);
	obs_data_set_string(service_settings, "key", streamKey);

	obs_service_update(service, service_settings);
	obs_service_apply_encoder_settings(service, vencoder_settings, aencoder_settings);

	// Configure output

	const char *output_type = obs_service_get_output_type(service);

	if (!output_type)
		output_type = "rtmp_output";

	obs_data_t* output_settings = obs_data_create();

	if (bindToIP) {
		obs_data_set_string(output_settings, "bind_ip", bindToIP);
	}

	obs_output_t* output = obs_output_create(output_type, "test_stream", output_settings, nullptr);

	// obs_output_update(output, output_settings);

	obs_output_set_video_encoder(output, vencoder);
	obs_output_set_audio_encoder(output, aencoder, 0);

	obs_output_set_service(output, service);

	// Setup output signal handler

	auto on_started = [](void* data, calldata_t*)
	{
		local_context* context = (local_context*)data;

		context->state = Running;
		context->signal_state_changed();
	};

	auto on_stopped = [](void* data, calldata_t*)
	{
		local_context* context = (local_context*)data;

		context->state = Stopped;
		context->signal_state_changed();
	};

	signal_handler *output_signal_handler = obs_output_get_signal_handler(output);
	signal_handler_connect(output_signal_handler, "start", on_started, &context);
	signal_handler_connect(output_signal_handler, "stop", on_stopped, &context);

	// Start testing

	bool success = false;

	if (obs_output_start(output))
	{
		context.wait_state_changed();

		if (context.state != Stopped)
		{
			// ignore first WARMUP_DURATION_MS due to possible buffering skewing
			// the result
			context.wait_state_changed(WARMUP_DURATION_MS);

			if (context.state == Running)
			{
				// Test bandwidth

				// Save start metrics
				uint64_t start_bytes = obs_output_get_total_bytes(output);
				uint64_t start_time_ns = os_gettime_ns();

				context.wait_state_changed((unsigned long)durationSeconds * 1000L);

				if (context.state == Running)
				{
					// Still running
					obs_output_stop(output);

					// Wait for stopped
					context.wait_state_changed();

					// Get end metrics
					uint64_t end_bytes = obs_output_get_total_bytes(output);
					uint64_t end_time_ns = os_gettime_ns();

					// Compute deltas
					uint64_t total_bytes = end_bytes - start_bytes;
					uint64_t total_time_ns = end_time_ns - start_time_ns;

					uint64_t total_bits = total_bytes * 8L;
					uint64_t total_time_seconds = total_time_ns / 1000000000L;

					resultBitsPerSecond = total_bits / total_time_seconds;

					if (obs_output_get_frames_dropped(output))
					{
						// Frames were dropped, use 70% of available bandwidth
						resultBitsPerSecond =
							resultBitsPerSecond * 70L / 100L;
					}
					else
					{
						// No frames were dropped, use 100% of available bandwidth
						resultBitsPerSecond =
							resultBitsPerSecond * 100L / 100L;
					}

					// Ceiling
					if (resultBitsPerSecond > maxBitrateBitsPerSecond)
						resultBitsPerSecond = maxBitrateBitsPerSecond;

					success = true;
				}
			}
		}
	}

	if (!success)
	{
		obs_output_force_stop(output);
	}

	signal_handler_disconnect(output_signal_handler, "start", on_started, &context);
	signal_handler_disconnect(output_signal_handler, "stop", on_started, &context);

	obs_output_release(output);
	obs_service_release(service);
	obs_encoder_release(vencoder);
	obs_encoder_release(aencoder);

	obs_data_release(output_settings);
	obs_data_release(service_settings);
	obs_data_release(vencoder_settings);
	obs_data_release(aencoder_settings);

	return resultBitsPerSecond;
}
