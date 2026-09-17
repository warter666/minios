# 05-Linux 设计文档：minios（mini-arm-os 风格裸机内核）

## 参照
jserv/mini-arm-os（1.3k★）在 QEMU lm3s6965evb（Cortex-M3）上的裸机内核课程；
xv6-riscv 的调度/系统调用思想在单核 M3 上的最小映射。

## 分层（自底向上）
1. **启动**：`start.S` 向量表（MSP 初值 + 15 个核心异常 + 外设 IRQ 槽位），
   `startup.c` 拷 .data / 清 .bss 后进 `main`。
2. **UART 驱动**：LM3S6965 PL011 类 UART0（0x4000C000），轮询式收发。
3. **上下文切换**：Cortex-M3 双栈机制——任务全部跑在 PSP 上，
   - 首次进入：main 在 MSP 上准备好 task0 的假栈帧（xPSR/PC/LR/R0-R3/R12/R4-R11），
     设 PSP 后 `svc 0`，SVC 处理程序只做"恢复半程"并以 0xFFFFFFFD 返回线程模式。
   - 抢占：SysTick（100Hz）置 PendSV pending；PendSV 保存 R4-R11 到任务栈
     （硬件帧由异常自动压入 PSP），C 函数 `pick_next` 选任务，恢复后 `bx lr`。
   - `tst lr, 0x10` 区分首次切换（EXC_RETURN=0xFFFFFFF9，帧在 MSP）与常规切换
     （0xFFFFFFFD，帧在 PSP）。
4. **调度**：朴素 round-robin，任务状态 RUNNABLE / SLEEPING / DONE；
   空闲任务常驻保底。
5. **系统调用**：`svc 0`，编号 r0、参数 r1/r2，帧指针即 PSP：
   `write`（UART 输出）、`getpid`、`sleep(ticks)`、`yield`、`exit`。
   sleep/exit 在返回前 pending PendSV，让处理器走完本异常再切换。
6. **用户面**：shell 任务轮询 UART 读行，支持 help / ps / echo / uptime；
   另一个任务每秒打一行心跳，用于证明多任务并发与 sleep 生效。

## 验证（QEMU 自动化）
`make run-test`：管道注入命令 → 超时杀 QEMU → 断言输出含
心跳行、ps 的任务表、echo 回显。工具链/仿真均在 WSL Ubuntu 内执行。

## 与 mini-arm-os 的差异
- 原课程每课一个目录渐进重写驱动；这里一次性整合成"可运行 shell"的最小完整内核。
- 调度做了任务状态机（sleep/exit），原课程的 sleepos 只演示单任务延时。
- 系统调用参数约定与 Linux syscall 类似（编号+两个参数），并支持返回值写回。
