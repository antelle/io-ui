#include "perf-trace.h"
#include <Windows.h>

double PerfTrace::TimeStamp() {
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return double(time.QuadPart);
}

double PerfTrace::Freq() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return double(freq.QuadPart) / 1000.0;
}
