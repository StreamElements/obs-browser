#pragma once

#include <string>
#include <vector>

#include <obs.h>

class StreamElementsBandwidthTestClient
{
private:
	// ignore first WARMUP_DURATION_MS due to possible buffering skewing
	// the result
	const unsigned long WARMUP_DURATION_MS = 2500L;

public:
	StreamElementsBandwidthTestClient();
	virtual ~StreamElementsBandwidthTestClient();

	uint64_t TestServerBitsPerSecond(
		const char* serverUrl,
		const char* streamKey,
		const int maxBitrateBitsPerSecond,
		const char* bindToIP = nullptr,
		const int durationSeconds = 10);
};
