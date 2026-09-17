#include "kernel.h"

/* ---- SVC (syscall) handling ---------------------------------------- */

/* called from start.S SVC branch with r0 = pointer to the stacked frame
   (on the task's PSP): r0 r1 r2 r3 r12 lr pc xpsr */
void svc_dispatch(uint32_t *frame)
{
    uint32_t num = frame[0];
    uint32_t arg1 = frame[1];
    uint32_t arg2 = frame[2];
    long ret = 0;
    switch (num) {
    case SYS_WRITE:
        uart_write_syscall((const char *)arg1, arg2);
        ret = (long)arg2;
        break;
    case SYS_GETPID:
        ret = task_current_id();
        break;
    case SYS_SLEEP:
        ret = 0;
        break;
    case SYS_EXIT:
        ret = 0;
        break;
    default:
        ret = -1;
        break;
    }
    frame[0] = (uint32_t)ret;
    if (num == SYS_SLEEP || num == SYS_EXIT) {
        if (num == SYS_SLEEP)
            task_sleep(arg1);
        else
            task_exit();
    }
}

__attribute__((naked)) void SVC_Handler(void)
{
    __asm volatile(
        "tst lr, #4           \n" /* SPSEL bit: 1 = frame on PSP (a task) */
        "beq svc_first_entry  \n" /* 0 = kernel context -> first task entry */
        "mrs r0, psp          \n"
        "b svc_dispatch_c     \n"
        "svc_first_entry:     \n"
        "b SVC_Entry_From_Kernel \n"
        "svc_dispatch_c:      \n"
        "b svc_dispatch       \n");
}

/* C-level syscall wrappers used by tasks */
long sys_write(const char *buf, uint32_t len)
{
    register long r0 __asm("r0") = SYS_WRITE;
    register long r1 __asm("r1") = (long)buf;
    register long r2 __asm("r2") = (long)len;
    __asm volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r2) : "memory");
    return r0;
}

long sys_getpid(void)
{
    register long r0 __asm("r0") = SYS_GETPID;
    __asm volatile("svc #0" : "+r"(r0) : : "memory");
    return r0;
}

void sys_sleep(uint32_t ticks)
{
    register long r0 __asm("r0") = SYS_SLEEP;
    register long r1 __asm("r1") = (long)ticks;
    __asm volatile("svc #0" : "+r"(r0) : "r"(r1) : "memory");
}

void sys_exit(void)
{
    register long r0 __asm("r0") = SYS_EXIT;
    __asm volatile("svc #0" : "+r"(r0) : : "memory");
}

/* ---- tiny formatting helpers (no libc) ----------------------------- */

static int streq(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

static int starts_with(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++)
            return 0;
    }
    return 1;
}

static void out_str(const char *s) { sys_write(s, 0xFFFFFFFF); }

/* sys_write with len=0xFFFFFFFF stops at NUL; svc dispatch passes len as arg2
   and uart_write_syscall treats that sentinel as "print to NUL" */
static void out_u32(uint32_t v)
{
    char buf[11];
    int i = 10;
    buf[i] = '\0';
    if (v == 0)
        buf[--i] = '0';
    while (v) {
        buf[--i] = '0' + (v % 10);
        v /= 10;
    }
    out_str(&buf[i]);
}

/* ---- tasks ---------------------------------------------------------- */

static void heartbeat_task(void)
{
    long pid = sys_getpid();
    for (;;) {
        out_str("[heartbeat] alive, tick ");
        out_u32(kernel_ticks());
        out_str("\n");
        (void)pid;
        sys_sleep(100); /* 100 * 10 ms = 1 s */
    }
}

static void run_command(const char *line)
{
    if (line[0] == '\0')
        return;
    if (streq(line, "help")) {
        out_str("commands: help, ps, uptime, echo <text>\n");
    } else if (streq(line, "ps")) {
        for (int i = 0; i < task_count(); i++) {
            out_str(task_name(i));
            out_str(": ");
            out_str(task_state_name(i));
            out_str("\n");
        }
    } else if (streq(line, "uptime")) {
        out_str("uptime: ");
        out_u32(kernel_ticks() / 100); /* seconds */
        out_str(" s\n");
    } else if (starts_with(line, "echo ")) {
        out_str(line + 5);
        out_str("\n");
    } else {
        out_str("unknown command; try help\n");
    }
}

static void shell_task(void)
{
    char line[64];
    int len = 0;
    out_str("minios shell (try 'help')\n");
    for (;;) {
        int c = uart_try_getc();
        if (c < 0) {
            sys_sleep(1); /* 10 ms poll interval */
            continue;
        }
        if (c == '\r' || c == '\n') {
            if (len) {
                line[len] = '\0';
                run_command(line);
                len = 0;
            }
            continue;
        }
        if (len < (int)sizeof(line) - 1)
            line[len++] = (char)c;
    }
}

static void idle_task(void)
{
    for (;;) {
        __asm volatile("wfi");
        sys_sleep(1);
    }
}

/* ---- kernel entry --------------------------------------------------- */

void kernel_main(void)
{
    uart_init();
    /* assume ~12 MHz core clock (QEMU lm3s6965evb default): 10 ms tick */
    SYST_RVR = 120000 - 1;
    SYST_CVR = 0;
    SYST_CSR = 7; /* CLKSOURCE | TICKINT | ENABLE */

    task_create(idle_task, "idle");
    task_create(shell_task, "shell");
    task_create(heartbeat_task, "heartbeat");

    /* enter the first task; nothing to save on the kernel stack yet */
    __asm volatile("msr psp, %0" ::"r"(task_first_sp()));
    __asm volatile("svc #0"); /* SVC_Entry_From_Kernel: jump into tasks[0] */

    for (;;) /* never reached */
        ;
}

/* SysTick: preemption point; wakes sleepers */
void SysTick_Handler(void)
{
    tasks_tick();
    SCB_ICSR |= ICSR_PENDSVSET;
}

/* ---- fault diagnostics --------------------------------------------- */

static void hex32_fault(uint32_t v)
{
    char buf[11] = "0x00000000\n";
    for (int i = 7; i >= 0; i--, v >>= 4)
        buf[2 + i] = "0123456789ABCDEF"[v & 0xF];
    for (char *p = buf; *p; p++) { /* polling, no syscalls inside a fault */
        while (UART_FR & UART_FR_TXFF)
            ;
        UART_DR = (uint8_t)*p;
    }
}

void hardfault_dump(void)
{
    volatile uint32_t *scb = (uint32_t *)0xE000ED28; /* CFSR HFSR DFSR MMFAR BFAR */
    const char *m = "\n[HARDFAULT]\n";
    while (*m) {
        while (UART_FR & UART_FR_TXFF)
            ;
        UART_DR = (uint8_t)*m++;
    }
    hex32_fault(scb[0]); /* CFSR */
    hex32_fault(scb[1]); /* HFSR */
    hex32_fault(scb[3]); /* MMFAR */
    hex32_fault(scb[4]); /* BFAR */
    uint32_t psp;
    __asm volatile("mrs %0, psp" : "=r"(psp));
    hex32_fault(psp);
    /* if the fault hit a task (thread PSP), the stacked frame is at psp:
       [0]r0 [1]r1 [2]r2 [3]r3 [4]r12 [5]lr [6]pc [7]xpsr */
    uint32_t *frame = (uint32_t *)psp;
    hex32_fault(frame[5]); /* lr */
    hex32_fault(frame[6]); /* pc  <- where the fault happened */
    hex32_fault(frame[7]); /* xpsr */
    for (;;)
        ;
}
