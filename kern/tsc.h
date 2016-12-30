/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_TSC_H
#define JOS_KERN_TSC_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/time.h>

void tsc_calibrate(void);
void timer_start(void);
void timer_stop(void);
void clock_init(void);

void clock_getres(int clock_id, struct timespec *res);
void clock_gettime(int clock_id, struct timespec *tp);
int clock_settime(int clock_id, const struct timespec *tp);

#endif	// !JOS_KERN_TSC_H
