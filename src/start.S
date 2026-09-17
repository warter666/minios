.syntax unified
.cpu cortex-m3
.thumb

.section .isr_vector, "a", %progbits
.global g_vector_table
g_vector_table:
    .word _stack_top            /* 0: initial MSP */
    .word Reset_Handler         /* 1: reset */
    .word HardFault_Handler     /* 2: NMI */
    .word HardFault_Handler     /* 3: hard fault */
    .word HardFault_Handler     /* 4: mem manage */
    .word HardFault_Handler     /* 5: bus fault */
    .word HardFault_Handler     /* 6: usage fault */
    .word 0                     /* 7 */
    .word 0                     /* 8 */
    .word 0                     /* 9 */
    .word 0                     /* 10 */
    .word SVC_Handler           /* 11: supervisor call */
    .word 0                     /* 12: debug monitor */
    .word 0                     /* 13 */
    .word PendSV_Handler        /* 14: pendable request */
    .word SysTick_Handler       /* 15: systick */
    .rept 32                    /* 16..47: external IRQ slots (GPIOA=0 .. UART0=5) */
    .word 0
    .endr

.section .text
.thumb_func
.global Reset_Handler
Reset_Handler:
    /* copy .data from flash to RAM */
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:  cmp r1, r2
    bcs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b
2:  /* zero .bss */
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
3:  cmp r1, r2
    bcs 4f
    str r3, [r1], #4
    b 3b
4:  bl kernel_main
5:  b 5b

.thumb_func
HardFault_Handler:
    bl hardfault_dump
6:  wfe
    b 6b

/* ---- context switch -------------------------------------------------- */

.global PendSV_Handler
.thumb_func
PendSV_Handler:
    cpsid i
    /* save callee-saved regs of the outgoing context (if it is a task on PSP) */
    mrs r0, psp
    tst lr, #4                  /* EXC_RETURN bit2 (SPSEL): 1 = frame on PSP */
    bne 1f
    b 2f                        /* bit2 clear -> context on MSP (kernel), nothing to save */
1:
    subs r0, #32
    stmia r0!, {r4-r7}
    mov r3, r8
    mov r4, r9
    mov r5, r10
    mov r6, r11
    stmia r0!, {r3-r6}
    subs r0, #32
    ldr r2, =g_current_task
    ldr r2, [r2]
    str r0, [r2]                /* tcb->sp */
2:
    bl pick_next_task           /* updates g_current_task (never returns NULL) */
    /* restore the incoming context: load r8-r11 first (via r4-r7 as scratch),
       then r4-r7 last -- loading them first would be clobbered */
    ldr r2, =g_current_task
    ldr r2, [r2]
    ldr r0, [r2]
    adds r0, #16
    ldmia r0!, {r4-r7}          /* task r8-r11 */
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7
    subs r0, #32
    ldmia r0!, {r4-r7}          /* task r4-r7 */
    adds r0, #16                /* skip r0-r3/r12/lr/pc/xpsr: hw pops them */
    msr psp, r0
    ldr r3, =0xFFFFFFFD         /* thread mode, PSP */
    cpsie i
    bx r3

/* first entry into the first task: kernel_main sets g_current_task and PSP,
   then traps here; there is nothing to save on the MSP side */
.global SVC_Entry_From_Kernel
.thumb_func
SVC_Entry_From_Kernel:
    ldr r2, =g_current_task
    ldr r2, [r2]
    ldr r0, [r2]
    adds r0, #16
    ldmia r0!, {r4-r7}          /* task r8-r11 */
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7
    subs r0, #32
    ldmia r0!, {r4-r7}          /* task r4-r7 */
    adds r0, #16                /* skip hw frame; exception return pops it */
    msr psp, r0
    ldr r3, =0xFFFFFFFD
    bx r3

/* task syscall trampoline: r0 = syscall number, r1/r2 = args */
.global svc_call
.thumb_func
svc_call:
    svc #0
    bx lr
