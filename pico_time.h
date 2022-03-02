/**
    @file pico_time.h
    @brief A simple time management library

    This library provides high-res time and sleep functions, as well as unit
    conversions functions.
*/

#ifndef PICO_TIME_H
#define PICO_TIME_H

#define _POSIX_C_SOURCE 199309L

#include <stdint.h>

/**
 * @brief Time value
 */
typedef uint64_t ptime_t;

/**
 * @brief Returns the present high-res clock time
 */
ptime_t pt_now();

/**
 * @brief Sleeps for at least the specified duration
 *
 * Note: On most platforms this function has microsecond resolution, except on
 * Windows where it only has millisecond resoultion.
 */
void pt_sleep(ptime_t duration);

/**
 * @brief Converts time to nanoseconds
 */
int64_t pt_to_nsec(ptime_t time);

/**
 * @brief Converts time to microseconds
 */
int64_t pt_to_usec(ptime_t time);

/**
 * @brief Converts time to milliseconds
 */
int32_t pt_to_msec(ptime_t time);

/**
 * @brief Converts time to seconds
 */
double  pt_to_sec(ptime_t time);

/**
 * @brief Make time from nanoseconds
 */
ptime_t pt_from_nsec(int64_t nsec);

/**
 * @brief Make time from microseconds
 */
ptime_t pt_from_usec(int64_t usec);

/**
 * @brief Make time from miliseconds
 */
ptime_t pt_from_msec(int32_t msec);

/**
 * @brief Time from miliseconds
 */
ptime_t pt_from_sec(double sec);

#endif // PICO_TIME_H

#ifdef PT_IMPLEMENTATION

#define PT_WINDOWS 1
#define PT_APPLE   2
#define PT_UNIX    3

#if defined(_WIN32) || defined(_WIN64) || defined (__CYGWIN__)
	#define PT_PLATFORM PT_WINDOWS
#elif defined(__APPLE__) && defined(__MACH__)
	#define PT_PLATFORM PT_APPLE
#elif defined(__unix__)
	#define PT_PLATFORM PT_UNIX
#else
    #error "Unsupported platform"
#endif

#if PT_PLATFORM == PT_WINDOWS

#include <windows.h>

ptime_t pt_now()
{
    static LARGE_INTEGER freq = { 0 };

    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);

    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);

    return (1000000UL * ticks.QuadPart) / freq.QuadPart;
}

void pt_sleep(ptime_t duration)
{
    TIMECAPS tc;
    timeGetDevCaps(&tc, sizeof(TIMECAPS));

    timeBeginPeriod(tc.wPeriodMin);
    Sleep(pt_to_msec(duration));
    timeEndPeriod(tc.wPeriodMin);
}

#elif PT_PLATFORM == PT_APPLE

#include <mach/mach_time.h>

ptime_t pt_now()
{
    static mach_timebase_info_data_t freq = { 0, 0 };

    if (freq.denom == 0)
        mach_timebase_info(&freq);

    uint64_t nsec = (mach_absolute_time() * freq.numer) / freq.denom;
    return nsec / 1000;
}

#elif PT_PLATFORM == PT_UNIX

#include <errno.h>
#include <time.h>

ptime_t pt_now()
{
    struct timespec ti;
    clock_gettime(CLOCK_MONOTONIC, &ti);
    return ti.tv_sec * 1000000UL + ti.tv_nsec / 1000;
}

#endif // PT_PLATFORM

#if PT_PLATFORM == PT_UNIX || PT_PLATFORM == PT_APPLE

#include <errno.h>
#include <time.h>

void pt_sleep(ptime_t duration)
{
    struct timespec ti;
    ti.tv_sec = duration / 1000000;
    ti.tv_nsec = (duration % 1000000) * 1000;

    while ((nanosleep(&ti, &ti) == -1) && (errno == EINTR));
}

#endif // PT_PLATFORM

int64_t pt_to_nsec(ptime_t time)
{
    return time * 1000;
}

int64_t pt_to_usec(ptime_t time)
{
    return time;
}

int32_t pt_to_msec(ptime_t time)
{
    return time / 1000;
}

double pt_to_sec(ptime_t time)
{
    return time / 1000000.0;
}

ptime_t pt_from_nsec(int64_t nsec)
{
    return nsec / 1000;
}

ptime_t pt_from_usec(int64_t usec)
{
    return usec;
}

ptime_t pt_from_msec(int32_t msec)
{
    return msec * 1000;
}

ptime_t pt_from_sec(double sec)
{
    return (ptime_t)(sec * 1000000.0);
}

#endif // PT_IMPLEMENTATION

