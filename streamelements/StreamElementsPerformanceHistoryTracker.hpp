#pragma once

#ifdef WIN32
#include <windows.h>
#endif

#include <util/threading.h>
#include <thread>
#include <mutex>
#include <vector>

class StreamElementsPerformanceHistoryTracker
{
public:
	typedef long double seconds_t;

	struct CpuTime
	{
		seconds_t totalSeconds;
		seconds_t idleSeconds;
		seconds_t busySeconds;
	};

	typedef CpuTime cpu_usage_t;
	#ifdef WIN32
	typedef MEMORYSTATUSEX memory_usage_t;
	#else
    struct memory_usage_t {
        unsigned int dwMemoryLoad; // Usage %
    };
	#endif

public:
	StreamElementsPerformanceHistoryTracker();
	~StreamElementsPerformanceHistoryTracker();

	std::vector<memory_usage_t> getMemoryUsageSnapshot();
	std::vector<cpu_usage_t> getCpuUsageSnapshot();

private:
	std::recursive_mutex m_mutex;

	os_event_t* m_quit_event;
	os_event_t* m_done_event;

	std::vector<cpu_usage_t> m_cpu_usage;
	std::vector<memory_usage_t> m_memory_usage;
};
