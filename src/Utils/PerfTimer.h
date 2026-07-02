#pragma once
#include <Windows.h>

/**
 * High-precision performance timer using QueryPerformanceCounter.
 *
 * Usage:
 *   PerfTimer t;
 *   // ... work ...
 *   double elapsedMs = t.ElapsedMs();  // e.g. 0.127 ms
 *
 * Replaces GetTickCount64() which has ~15.6ms resolution and
 * hides real latency in sub-tick operations.
 */
class PerfTimer
{
public:
    PerfTimer() : _start({}), _freq({})
    {
        QueryPerformanceFrequency(&_freq);
        QueryPerformanceCounter(&_start);
    }

    /** Milliseconds elapsed since construction, with sub-microsecond precision. */
    double ElapsedMs() const
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return static_cast<double>(now.QuadPart - _start.QuadPart) * 1000.0 /
               static_cast<double>(_freq.QuadPart);
    }

    /** Reset the start point to now. */
    void Reset()
    {
        QueryPerformanceCounter(&_start);
    }

private:
    LARGE_INTEGER _start;
    LARGE_INTEGER _freq;
};
