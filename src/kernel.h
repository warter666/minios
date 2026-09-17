#pragma once
#include <stdint.h>

/* ---- LM3S6965 UART0 (PL011-like) ---- */
#define UART0_BASE 0x4000C000u
#define UART_DR    (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART_FR    (*(volatile uint32_t *)(UART0_BASE + 0x18))
#define UART_IBRD  (*(volatile uint32_t *)(UART0_BASE + 0x24))
#define UART_FBRD  (*(volatile uint32_t *)(UART0_BASE + 0x28))
#define UART_LCRH  (*(volatile uint32_t *)(UART0_BASE + 0x2C))
#define UART_CTL   (*(volatile uint32_t *)(UART0_BASE + 0x30))
#define UART_FR_TXFF (1u << 5)
#define UART_FR_RXFE (1u << 4)

/* ---- SysTick / SCB / NVIC ---- */
#define SYST_CSR  (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR  (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR  (*(volatile uint32_t *)0xE000E018)
#define SCB_ICSR  (*(volatile uint32_t *)0xE000ED04)
#define ICSR_PENDSVSET (1u << 28)
#define NVIC_ISER0 (*(volatile uint32_t *)0xE000E100)

/* ---- task states ---- */
enum task_state { TASK_RUNNABLE = 0, TASK_SLEEPING, TASK_DONE };

#define MAX_TASKS 8
#define TASK_STACK_WORDS 512 /* 2 KiB per task */

typedef struct {
    volatile uint32_t *sp;
    enum task_state state;
    uint32_t wake_at;        /* tick at which a sleeping task wakes */
    const char *name;
    uint32_t stack[TASK_STACK_WORDS];
} tcb_t;

/* ---- syscalls (r0 = number, r1/r2 = args) ---- */
enum syscall_num {
    SYS_WRITE = 1,   /* write(fd ignored, buf, len) -> len */
    SYS_GETPID = 2,  /* -> task id */
    SYS_SLEEP = 3,   /* sleep(ticks) -> 0 */
    SYS_EXIT = 4,    /* exit(code) -> n/a */
};

/* task.c */
extern volatile tcb_t *g_current_task;
void pick_next_task(void);
int task_create(void (*entry)(void), const char *name);
void task_sleep(uint32_t ticks);
void task_exit(void);
int task_current_id(void);
void tasks_tick(void); /* called from SysTick: wake sleepers */
int task_count(void);
const char *task_name(int id);
const char *task_state_name(int id);
uint32_t task_first_sp(void);

/* uart.c */
void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
int uart_try_getc(void); /* -1 when empty */

/* kernel.c */
void kernel_main(void);
uint32_t kernel_ticks(void);
void uart_write_syscall(const char *buf, uint32_t len);
