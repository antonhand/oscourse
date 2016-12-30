
#include <inc/stdio.h>
#include <inc/assert.h>

#ifndef JOS_INC_TIME_H
#define JOS_INC_TIME_H

struct tm
{
    int tm_sec;                   /* Seconds.     [0-60] */
    int tm_min;                   /* Minutes.     [0-59] */
    int tm_hour;                  /* Hours.       [0-23] */
    int tm_mday;                  /* Day.         [1-31] */
    int tm_mon;                   /* Month.       [0-11] */
    int tm_year;                  /* Year - 1900.  */
};

struct timespec
{
    int tv_sec;
    long tv_nsec;
};

enum {
    CLOCK_MONOTONIC = 0,
    CLOCK_REALTIME,
    CLOCK_PROCESS_CPUTIME_ID,
    CLOCK_NUM
};

#define TIMER_ABSTIME 1

int is_leap_year(int year);

int d_to_s(int d);

int timestamp(struct tm *time);

void mktime(int time, struct tm *tm);

void print_datetime(struct tm *tm);

void snprint_datetime(char *buf, int size, struct tm *tm);

struct timespec add_timespec(const struct timespec *tm1, const struct timespec *tm2);

struct timespec sub_timespec(const struct timespec *tm1, const struct timespec *tm2);

int clock_nanosleep(int clock_id, int flags, const struct timespec *rqtp, struct timespec *rmtp);

#endif
