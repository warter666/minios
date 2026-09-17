# minios — mini-arm-os 风格裸机内核重写

在 QEMU lm3s6965evb（Cortex-M3）上从零实现的抢占式多任务内核，含一个可交互的 shell。
参照 [jserv/mini-arm-os](https://github.com/jserv/mini-arm-os)（1.3k★）的课程脉络与
xv6 的调度/系统调用思想。

## 结构（自底向上）

| 层 | 文件 | 内容 |
|----|------|------|
| 启动 | `src/start.S` | 向量表、Reset（拷 .data/清 .bss）、PendSV/SVC 上下文切换汇编 |
| 驱动 | `src/uart.c` | PL011 类 UART0 轮询收发 |
| 内核 | `src/task.c` | TCB/任务栈帧构造、round-robin 调度、sleep 唤醒 |
| 内核 | `src/kernel.c` | 系统调用分发、shell/heartbeat/idle 任务、HardFault 诊断 |
| 构建 | `linker.ld` `Makefile` | flash@0x0 / SRAM@0x20000000，arm-none-eabi-gcc |

## 关键机制

- **双栈上下文切换**：任务全部运行在 PSP；首次进入由 kernel 设 PSP 后 `svc 0`，
  handler 以 `EXC_RETURN=0xFFFFFFFD` 返回线程模式。判断上下文在 PSP 还是 MSP 用
  `tst lr, #4`（EXC_RETURN bit2/SPSEL）——**不是 bit4**。
- **抢占**：SysTick 100Hz → 唤醒到期任务 → 置 PendSV pending（与 SVC 尾链）。
  PendSV 软件保存/恢复 R4-R11，硬件帧（r0-r3,r12,lr,pc,xpsr）由异常机制入栈出栈。
- **系统调用**：`svc 0`，编号在 r0、参数 r1/r2，返回值写回栈帧 r0。
  提供 write / getpid / sleep / exit。

## 踩过的三个坑（都由 QEMU + HardFault 诊断定位）

1. **EXC_RETURN 判断位用错**：`tst lr, #0x10` 应为 `tst lr, #4`。bit4 在
   0xFFFFFFF9（thread-MSP）上也为 1，导致内核上下文的 r4-r11 被当作任务上下文
   存进任务栈（栈里出现 SysTick 重装常量 0x1D4BF 是破案线索）。
2. **恢复时寄存器被自己覆盖**：`ldmia {r4-r7}` 后再 `ldmia {r3-r6}` 会把
   r4-r6 覆盖成任务的 r8-r10。必须先借 r4-r7 取 r8-r11 搬入高位寄存器，
   最后再取 r4-r7。
3. **诊断程序自己 fault**：HardFault 里调 `sys_write`（svc）会因优先级 -1 无法
   升级而 lockup。fault 路径必须直接轮询 UART。

## 运行与测试（WSL Ubuntu 内）

```bash
make            # 需要 arm-none-eabi-gcc
make run        # 交互式体验（Ctrl-A X 退出 QEMU）
bash test.sh    # 自动化冒烟：注入命令并断言输出
```
