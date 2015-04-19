#include "perf-trace.h"
#include <sys/time.h>
#include <iostream>

double PerfTrace::Freq() {
    return 1.0;
}

double PerfTrace::TimeStamp() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return double((ts.tv_sec * 1000.0) + (ts.tv_nsec / 1000000.0));
}
