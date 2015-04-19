#include "perf-trace.h"

using namespace v8;

double PerfTrace::TimeStamps[32] = { 0 };
double PerfTrace::_timeFreq = 0;
double PerfTrace::_first = 0;

PerfTrace::PerfTrace() {
}

PerfTrace::PerfTrace(PerfTrace& pt) {
}

void PerfTrace::RegV8(const FunctionCallbackInfo<Value>& args) {
    UI_PERF_EVENT evt = (UI_PERF_EVENT)args[0]->IntegerValue();
    PerfTrace::Reg(evt);
}

void PerfTrace::Reg(UI_PERF_EVENT evt) {
    if (PerfTrace::TimeStamps[evt] > 0)
        return;
    double time = PerfTrace::TimeStamp();
    if (evt == UI_PERF_EVENT_START) {
        _timeFreq = PerfTrace::Freq();
        _first = time / _timeFreq;
        PerfTrace::TimeStamps[0] = 0;
    }
    else {
        if (_timeFreq > 0) {
            PerfTrace::TimeStamps[evt] = time / _timeFreq - _first;
        }
    }
}
