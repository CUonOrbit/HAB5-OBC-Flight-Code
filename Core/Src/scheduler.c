#include "scheduler.h"

static Task_t tasks[SCHEDULER_MAX_TASKS];
static int    task_count = 0;

void Scheduler_Init(void)
{
    task_count = 0;
    for (int i = 0; i < SCHEDULER_MAX_TASKS; i++)
    {
        tasks[i].callback    = NULL;
        tasks[i].interval_ms = 0;
        tasks[i].last_run    = 0;
        tasks[i].name        = "";
        tasks[i].enabled     = 0;
    }
}

int Scheduler_AddTask(TaskCallback_t callback, uint32_t interval_ms, const char *name)
{
    if (task_count >= SCHEDULER_MAX_TASKS || callback == NULL)
    {
        return -1;
    }

    int idx = task_count;
    tasks[idx].callback    = callback;
    tasks[idx].interval_ms = interval_ms;
    tasks[idx].last_run    = HAL_GetTick();
    tasks[idx].name        = name;
    tasks[idx].enabled     = 1;
    task_count++;

    return idx;
}

void Scheduler_SetEnabled(int task_index, uint8_t enabled)
{
    if (task_index < 0 || task_index >= task_count)
    {
        return;
    }
    tasks[task_index].enabled = enabled;
}

void Scheduler_Update(void)
{
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < task_count; i++)
    {
        if (!tasks[i].enabled)
        {
            continue;
        }
        if ((now - tasks[i].last_run) >= tasks[i].interval_ms)
        {
            tasks[i].last_run = now;
            tasks[i].callback();
        }
    }
}