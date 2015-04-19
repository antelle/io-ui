#include "perf-trace.h"
#include <mach/mach_time.h>

double PerfTrace::Freq() {
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    return 1e6 * (double)timebase.denom / (double)timebase.numer;
}

double PerfTrace::TimeStamp() {
    return double(mach_absolute_time());
}
