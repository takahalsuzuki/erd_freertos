#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "GPIO.hpp"
#include <array>

#define min(_a_, _b_) ((_a_ < _b_) ? (_a_) : (_b_))
#define max(_a_, _b_) ((_a_ > _b_) ? (_a_) : (_b_))

#define MY_TICK (13000 - 525)

#define TEST_1
// #define EVAL_RM

struct task_arg {
    const char *name;
    int wcet;
    int period;
    int priority;
    bool priv;
    int bcrt;
    int wcrt;
};

struct scb_t {
    int             wcet;           // WCET of VS
    int             period;         // period of VS
    int             r;              // WCRT of VS
    UBaseType_t     vs_pri;         // priority of VS
    UBaseType_t     base_pri;       // priority of priv_t

    TaskHandle_t    priv_t;
    TickType_t  svs;                // start time of vs
    configRUN_TIME_COUNTER_TYPE dst; // deligation start tick of priv_t
};

#ifdef TEST_1
/* In ERD-light, WCRT of t3 is more bigger than DM */
#define HYPER_PERIOD 8400 * 10
task_arg task_set[] = {
    { "t1", 2000,  4000, 3, false },
    { "t2", 3000, 12000, 1, false },
    { "t3", 3000, 14000, 0, true },
};

struct scb_t scb = {
    /* in actual RTA, R(VS) = 7
     * but lerax this value dut to period is same as t2 */
    3000, 12000, 12000, 2, 0, 
};
#endif

#ifdef TEST_2
#define HYPER_PERIOD 7000 * 10
task_arg task_set[] = {
    { "t1", 2000,  5000, 2, false },
    { "t2", 2000,  7000, 1, false },
    { "t3", 2000, 10000, 0, true },
};

struct scb_t scb = {
    1000, 5000, 1000, 3, 0, 
};
#endif

#ifdef TEST_3
#define HYPER_PERIOD 39000 * 10
task_arg task_set[] = {
    { "t1", 1000,  5000, 2, false },
    { "t2", 2000,  6000, 1, false },
    { "t3", 4000, 13000, 0, true },
};

struct scb_t scb = {
    2000, 5000, 2000, 3, 0, 
};
#endif

#ifdef TEST_4
#define HYPER_PERIOD 84000 * 10
task_arg task_set[] = {
    { "t1", 1000,  5000, 4, false },
    { "t2", 1000,  6000, 3, false },
    { "t3", 2000,  8000, 1, false },
    { "t4", 4000, 14000, 0, true },
};

struct scb_t scb = {
    2000, 8000, 4000, 2, 0, 
};
#endif

static configRUN_TIME_COUNTER_TYPE
get_consumed(TaskHandle_t t)
{
    static TaskStatus_t status;
    vTaskGetInfo(t, &status, pdFALSE, eInvalid);
    return status.ulRunTimeCounter;
}


void
periodic_task(void *p)
{
    struct task_arg *t = (struct task_arg *)p;

    char *name = pcTaskGetName(NULL);
    TickType_t periodTick;
    TickType_t rel, t1, t2;

    periodTick = 0;
    while (true) {
        volatile int i;

        t1 = xTaskGetTickCount();
        rel = int(t1 / t->period) * t->period;
        printf("%s (%8.3f)\n", name, (float) rel / 1000);

        // simulate consume until WCET
        for (i = 0; i < t->wcet * MY_TICK; i++) {
        }

        t2 = xTaskGetTickCount();
        printf( "%s (%8.3f) fin\n", name, (float) t2 / 1000);

        t->bcrt = min(t->bcrt, t2 - rel);
        t->wcrt = max(t->wcrt, t2 - rel);
        vTaskDelayUntil(&periodTick, t->period);
    }
}

void
stats_task(void *p)
{
#define BUFFER_SIZE 512
    static char stat_buffer[BUFFER_SIZE];

    vTaskGetRunTimeStats(stat_buffer);
    printf("%s\n", stat_buffer);

    for (task_arg & ta : task_set) {
        printf("%s : %d - %d\n", ta.name, ta.bcrt, ta.wcrt);
    }

    vTaskSuspendAll();
}

void
vs_cb(TimerHandle_t xTimer)
{
#ifndef EVAL_RM
    scb.svs = xTaskGetTickCount();
    scb.dst = get_consumed(scb.priv_t);
    vTaskPrioritySet(scb.priv_t, scb.vs_pri);
#endif // EVAL_RM
}

void
timer_cb(TimerHandle_t xTimer)
{
    TickType_t  now = xTaskGetTickCountFromISR();
#ifndef EVAL_RM
    if (((scb.svs + scb.r - 5) <= now) ||
        ((get_consumed(scb.priv_t) - scb.dst + 5) >= scb.wcet)) { 
        vTaskPrioritySet(scb.priv_t, scb.base_pri);
    }
#endif // EVAL_RM

    if (now >= HYPER_PERIOD) {
        xTaskCreate(stats_task, "stat", 256, task_set, 5, NULL);
    }
}

int main()
{
    TaskHandle_t priv_t;
    TimerHandle_t t1, t2;

    gpio_init(12);
    gpio_set_dir(12, GPIO_OUT);
    gpio_init(14);
    gpio_set_dir(14, GPIO_OUT);

    setup_default_uart();

    printf("Start ERD simulation\n");

    for (task_arg& ta : task_set) {
        ta.bcrt = ta.period;
        xTaskCreate(periodic_task, ta.name, 256, &ta, ta.priority, ta.priv ? &priv_t : NULL);
    }

    t1 = xTimerCreate("vs", scb.period, pdTRUE, 0, vs_cb);
    t2 = xTimerCreate("tick", 100, pdTRUE, 0, timer_cb);
    xTimerStart(t2, 0);
    xTimerStart(t1, 0);

    scb.priv_t = priv_t;

    // invoke VS        
    vs_cb(NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
