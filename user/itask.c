#include <inc/lib.h>
#include <inc/time.h>

int test_clock_gettime(void)
{
    struct timespec tp;

    if(!sys_clock_gettime(500, &tp) || !sys_clock_gettime(-1, &tp)){
        return 1;
    }
    
    sys_clock_gettime(CLOCK_REALTIME, &tp);

    int diff = tp.tv_sec - sys_gettime();
    
    if(diff > 1 || diff < -1){
        return 1;
    }

    int clock;

    for(clock = CLOCK_MONOTONIC; clock <= CLOCK_PROCESS_CPUTIME_ID; clock++){
        struct timespec tp1, tp2;
        int i;

        int timestart = sys_gettime();

        sys_clock_gettime(clock, &tp1);
        for(i = 0; i < 1000000000; i++);
        
        diff = sys_gettime() - timestart;

        sys_clock_gettime(clock, &tp2);

        int diff1 = tp2.tv_sec - tp1.tv_sec;

        diff -= diff1;

        if(diff > 1 || diff < -1){
            return 1;
        }
    }

    return 0;
}

int test_clock_getres(void)
{
    struct timespec tp1, tp2, tp3;

    sys_clock_getres(CLOCK_MONOTONIC, &tp1);
    sys_clock_getres(CLOCK_REALTIME, &tp2);
    sys_clock_getres(CLOCK_PROCESS_CPUTIME_ID, &tp3);

    if(tp1.tv_sec != tp2.tv_sec
    || tp2.tv_sec != tp3.tv_sec
    || tp1.tv_nsec != tp2.tv_nsec
    || tp2.tv_nsec != tp3.tv_nsec){
        return 1;
    }

    return 0;
}

int test_clock_settime(void)
{
    struct timespec tp;

    tp.tv_sec = 1;
    tp.tv_nsec = 0;

    if(!sys_clock_settime(500, &tp)
    || !sys_clock_settime(-1, &tp)
    || !sys_clock_settime(-1, &tp)
    || !sys_clock_settime(CLOCK_MONOTONIC, &tp)){
        return 1;
    }

    int clock;

    tp.tv_sec = -1;
    tp.tv_nsec = 0;     

    for(clock = CLOCK_REALTIME; clock <= CLOCK_PROCESS_CPUTIME_ID; clock++){
        if(!sys_clock_settime(clock, &tp)){
            return 1;
        }
    }

    tp.tv_sec = 1;
    tp.tv_nsec = -1;     

    for(clock = CLOCK_REALTIME; clock <= CLOCK_PROCESS_CPUTIME_ID; clock++){
        if(!sys_clock_settime(clock, &tp)){
            return 1;
        }
    }

    tp.tv_sec = 1;
    tp.tv_nsec = 1000000000;     

    for(clock = CLOCK_REALTIME; clock <= CLOCK_PROCESS_CPUTIME_ID; clock++){
        if(!sys_clock_settime(clock, &tp)){
            return 1;
        }
    }

    tp.tv_sec = 1;
    tp.tv_nsec = 0;     

    for(clock = CLOCK_REALTIME; clock <= CLOCK_PROCESS_CPUTIME_ID; clock++){
        sys_clock_settime(clock, &tp);

        struct timespec tp1;
        int diff, i;

        int timestart = sys_gettime();

        for(i = 0; i < 1000000000; i++);
        
        diff = sys_gettime() - timestart;

        sys_clock_gettime(clock, &tp1);

        int diff1 = tp.tv_sec + diff - tp1.tv_sec;

        if(diff1 > 1 || diff1 < -1){
            return 1;
        }
    }

    for(clock = CLOCK_REALTIME; clock <= CLOCK_PROCESS_CPUTIME_ID; clock++){
        sys_clock_getres(clock, &tp);

        struct timespec tp1 = tp;

        tp1.tv_nsec += 1;

        sys_clock_settime(clock, &tp1);

        for(i = 0; i < 1000000000; i++);

        sys_clock_gettime(clock, &tp1);

        if(tp1.tv_nsec % tp.tv_nsec){
            return 1;
        }
    }

    return 0;
}

#define LAMBDA 100

int test_clock_nanosleep(void)
{
    struct timespec tp;
    int clock;

    for(clock = CLOCK_MONOTONIC; clock <= CLOCK_PROCESS_CPUTIME_ID; clock++){
        sys_clock_gettime(clock, &tp);

        tp.tv_sec += 4;
        clock_nanosleep(clock, TIMER_ABSTIME, &tp, NULL);

        struct timespec tp1;

        sys_clock_gettime(clock, &tp1);

        long diff = tp1.tv_nsec - tp.tv_nsec;

        if(tp.tv_sec != tp1.tv_sec || diff > LAMBDA){
            return 1;
        }
    }

    for(clock = CLOCK_MONOTONIC; clock <= CLOCK_PROCESS_CPUTIME_ID; clock++){
        tp.tv_sec = 3;
        tp.tv_nsec = 361462;

        struct timespec tp1, tp2;

        sys_clock_gettime(clock, &tp1);

        clock_nanosleep(clock, 0, &tp, NULL);

        sys_clock_gettime(clock, &tp2);

        long diff = tp2.tv_nsec - tp1.tv_nsec;

        if(tp2.tv_sec != tp1.tv_sec || diff > LAMBDA){
            return 1;
        }
    }

    for(clock = CLOCK_REALTIME; clock <= CLOCK_PROCESS_CPUTIME_ID; clock++){
        int abstime;
        for(abstime = TIMER_ABSTIME; abstime < TIMER_ABSTIME - 2; abstime--){
            struct timespec tp1, tp2;

            sys_clock_gettime(clock, &tp);

            tp1 = tp;
            
            if(abstime == TIMER_ABSTIME){
                tp.tv_sec += 4;
            } else {
                tp.tv_sec = 4;
                tp.tv_nsec = 4;
            }

            if(fork()){
                if(abstime != TIMER_ABSTIME){
                    sys_clock_gettime(CLOCK_MONOTONIC, &tp1);
                }
                clock_nanosleep(clock, abstime, &tp, NULL);
            } else {
                for(i = 0; i < 100000; i++);
                sys_clock_settime(clock, &tp);
                exit();
            }

            sys_clock_gettime(CLOCK_MONOTONIC, &tp2);

            int diff = tp2.tv_sec - tp1.tv_sec;

            if((diff >= 4 && abstime == TIMER_ABSTIME)
            || (diff < 4 && abstime != TIMER_ABSTIME)){
                return 1;
            }
        }
    }

    return 0;
}

void
umain(int argc, char **argv)
{
    int score = 0;

    cprintf("clock_gettime: ");
    if(test_clock_gettime()){
        cprintf("FAIL\n");
    } else {
        cprintf("OK\n");
        score += 30;
    }

    cprintf("clock_getres: ");
    if(test_clock_getres()){
        cprintf("FAIL\n");
    } else {
        cprintf("OK\n");
        score += 10;
    }

    cprintf("clock_settime: ");
    if(test_clock_settime()){
        cprintf("FAIL\n");
    } else {
        cprintf("OK\n");
        score += 30;
    }

    cprintf("clock_nanosleep: ");
    if(test_clock_nanosleep()){
        cprintf("FAIL\n");
    } else {
        cprintf("OK\n");
        score += 30;
    }

    cprintf("Score: %d/100\n", score);
}
