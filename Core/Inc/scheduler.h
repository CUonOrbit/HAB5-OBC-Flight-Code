#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "main.h"

#define SCHEDULER_MAX_TASKS 16

typedef void (*TaskCallback_t)(void);

typedef struct {
    TaskCallback_t callback;
    uint32_t       interval_ms;
    uint32_t       last_run;
    const char    *name;      /* handy for debugging/printf */
    uint8_t        enabled;
} Task_t;

void Scheduler_Init(void);
/* Returns the task's index, or -1 if the task table is full */
int  Scheduler_AddTask(TaskCallback_t callback, uint32_t interval_ms, const char *name);
void Scheduler_SetEnabled(int task_index, uint8_t enabled);
void Scheduler_Update(void);

#endif /* SCHEDULER_H */