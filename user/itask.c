#include <inc/lib.h>


#define LAMBDA 50

int test_clock_gettime(void)
{
    struct timespec tp;

    if(!sys_clock_gettime(500, &tp) || !sys_clock_gettime(-1, &tp)){
        return 1;
    }
    
    sys_clock_gettime(CLOCK_REALTIME, &tp);

    int diff = tp.tv_sec - sys_gettime();
    
    if(diff > 1 || diff < -1){
        cprintf("diff %d %d %d\n", diff, tp.tv_sec, sys_gettime());
        return 2;
    }

    int clock;

    for(clock = CLOCK_MONOTONIC; clock <= CLOCK_REALTIME; clock++){
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
            return 3;
        }
    }

    int k, prev_s = 0;
    long prev_ns = 0;
    for(k = 0; k < 6; k++){
        struct timespec tp1, tp2;
        int i;

        sys_clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp1);
        for(i = 0; i < 1000000; i++);

        sys_clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp2);

        diff = tp2.tv_sec - tp1.tv_sec;
        int diff1 = tp2.tv_nsec - tp1.tv_nsec;

        if(prev_s && prev_ns && ((diff - prev_s) || diff1 - prev_ns >  LAMBDA || diff1 - prev_ns < -LAMBDA)){
            return 4;
        }
        prev_s = diff;
        prev_ns = (prev_ns * k + diff1) / (k + 1);
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
            return 2;
        }
    }

    tp.tv_sec = 1;
    tp.tv_nsec = -1;     

    for(clock = CLOCK_REALTIME; clock <= CLOCK_PROCESS_CPUTIME_ID; clock++){
        if(!sys_clock_settime(clock, &tp)){
            return 3;
        }
    }

    tp.tv_sec = 1;
    tp.tv_nsec = 1000000000;     

    for(clock = CLOCK_REALTIME; clock <= CLOCK_PROCESS_CPUTIME_ID; clock++){
        if(!sys_clock_settime(clock, &tp)){
            return 4;
        }
    }

    tp.tv_sec = 1;
    tp.tv_nsec = 0;     

    for(clock = CLOCK_REALTIME; clock <= CLOCK_PROCESS_CPUTIME_ID; clock++){
        struct timespec tp1;
        sys_clock_settime(clock, &tp);
        sys_clock_gettime(clock, &tp1);

        tp1 = sub_timespec(&tp1, &tp);

        if(tp1.tv_sec){
            return 5;
        }
    }

    for(clock = CLOCK_REALTIME; clock <= CLOCK_PROCESS_CPUTIME_ID; clock++){
        sys_clock_getres(clock, &tp);

        struct timespec tp1 = tp;

        tp1.tv_nsec += 1;
        int i;

        sys_clock_settime(clock, &tp1);

        for(i = 0; i < 1000000; i++);

        sys_clock_gettime(clock, &tp1);

        if(tp1.tv_nsec % tp.tv_nsec){
            return 6;
        }
    }

    return 0;
}

int test_clock_nanosleep(void)
{
    struct timespec tp;
    int clock;

    if(!sys_clock_nanosleep(CLOCK_PROCESS_CPUTIME_ID, 0, &tp, NULL)){
        return 1;
    }

    for(clock = CLOCK_MONOTONIC; clock <= CLOCK_REALTIME; clock++){
        struct timespec tp1;
        sys_clock_gettime(clock, &tp);

        tp.tv_sec += 4;
        sys_clock_nanosleep(clock, TIMER_ABSTIME, &tp, NULL);
        sys_clock_gettime(clock, &tp1);

        tp = sub_timespec(&tp1, &tp);

        if(tp.tv_sec){
            return 2;
        }
    }

    for(clock = CLOCK_MONOTONIC; clock <= CLOCK_REALTIME; clock++){
        tp.tv_sec = 3;
        tp.tv_nsec = 361462;

        struct timespec tp1, tp2;

        sys_clock_gettime(clock, &tp1);

        sys_clock_nanosleep(clock, 0, &tp, NULL);

        sys_clock_gettime(clock, &tp2);

        tp = sub_timespec(&tp2, &tp1);

        if(tp.tv_sec > 3){
            return 3;
        }
    }

    int abstime;
    for(abstime = TIMER_ABSTIME; abstime > TIMER_ABSTIME - 2; abstime--){
        struct timespec tp1, tp2;

        sys_clock_gettime(CLOCK_REALTIME, &tp);

        sys_clock_gettime(CLOCK_MONOTONIC, &tp1);

        if(abstime == TIMER_ABSTIME){
            tp.tv_sec += 4;
        } else {
            tp.tv_sec = 4;
            tp.tv_nsec = 4;
        }
        if(fork()){
            sys_clock_nanosleep(CLOCK_REALTIME, abstime, &tp, NULL);
        } else {
            sys_clock_settime(CLOCK_REALTIME, &tp);
            exit();
        }

        sys_clock_gettime(CLOCK_MONOTONIC, &tp2);

        tp = sub_timespec(&tp2, &tp1);

        if((tp.tv_sec >= 4 && abstime == TIMER_ABSTIME)
        || (tp.tv_sec < 4 && abstime != TIMER_ABSTIME)){
            cprintf("abs %d tv_sec %d\n", abstime, tp.tv_sec);
            return 4;
        }
    }

    return 0;
}

void
umain(int argc, char **argv)
{
    int err, score = 0;

    cprintf("clock_gettime: ");
    if((err = test_clock_gettime())){
        cprintf("FAIL\nError code: %d\n", err);
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
    if((err = test_clock_settime())){
        cprintf("FAIL\nError code: %d\n", err);
    } else {
        cprintf("OK\n");
        score += 30;
    }

    cprintf("clock_nanosleep: ");
    if((err = test_clock_nanosleep())){
        cprintf("FAIL\nError code: %d\n", err);
    } else {
        cprintf("OK\n");
        score += 30;
    }

    cprintf("Score: %d/100\n", score);
}
