#include "kernel.h"

volatile tcb_t *g_current_task;

static tcb_t tasks[MAX_TASKS];
static int n_tasks;
static uint32_t g_ticks;

/* build the initial stack exactly as the exception machinery leaves it:
   [.. r4 r5 r6 r7 r8 r9 r10 r11 | r0 r1 r2 r3 r12 lr pc xpsr] <- top */
static void stack_init(tcb_t *t, void (*entry)(void))
{
    uint32_t *sp = t->stack + TASK_STACK_WORDS;
    *--sp = 0x01000000;                 /* xPSR: thumb bit */
    *--sp = (uint32_t)entry;            /* PC */
    *--sp = 0xFFFFFFFF;                 /* LR */
    *--sp = 0;                          /* r12 */
    *--sp = 0; *--sp = 0; *--sp = 0;    /* r3 r2 r1 */
    *--sp = 0;                          /* r0 */
    sp -= 8;                            /* r4-r11 */
    t->sp = sp;
}

int task_create(void (*entry)(void), const char *name)
{
    if (n_tasks >= MAX_TASKS)
        return -1;
    tcb_t *t = &tasks[n_tasks];
    t->state = TASK_RUNNABLE;
    t->wake_at = 0;
    t->name = name;
    stack_init(t, entry);
    return n_tasks++;
}

/* round-robin over runnable tasks; falls back to staying on current */
void pick_next_task(void)
{
    int cur = g_current_task ? (int)(g_current_task - tasks) : 0;
    for (int k = 1; k <= n_tasks; k++) {
        int i = (cur + k) % n_tasks;
        if (tasks[i].state == TASK_RUNNABLE) {
            g_current_task = &tasks[i];
            return;
        }
    }
    g_current_task = &tasks[cur]; /* nobody else runnable */
}

void task_sleep(uint32_t ticks)
{
    tcb_t *t = (tcb_t *)g_current_task;
    if (ticks == 0) {
        SCB_ICSR |= ICSR_PENDSVSET; /* yield */
        return;
    }
    t->state = TASK_SLEEPING;
    t->wake_at = g_ticks + ticks;
    SCB_ICSR |= ICSR_PENDSVSET;     /* switch away; systick wakes us */
}

void task_exit(void)
{
    ((tcb_t *)g_current_task)->state = TASK_DONE;
    SCB_ICSR |= ICSR_PENDSVSET;
}

int task_current_id(void)
{
    return (int)(g_current_task - tasks);
}

void tasks_tick(void)
{
    g_ticks++;
    for (int i = 0; i < n_tasks; i++) {
        if (tasks[i].state == TASK_SLEEPING && g_ticks >= tasks[i].wake_at)
            tasks[i].state = TASK_RUNNABLE;
    }
}

uint32_t kernel_ticks(void) { return g_ticks; }

/* scheduler bootstrap: select task 0 and expose its prepared stack */
uint32_t task_first_sp(void)
{
    g_current_task = &tasks[0];
    return (uint32_t)tasks[0].sp;
}

/* introspection for the shell's `ps` */
int task_count(void) { return n_tasks; }
const char *task_name(int id) { return tasks[id].name; }

const char *task_state_name(int id)
{
    switch (tasks[id].state) {
    case TASK_RUNNABLE: return "runnable";
    case TASK_SLEEPING: return "sleeping";
    case TASK_DONE:     return "done";
    }
    return "?";
}
