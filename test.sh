#!/usr/bin/env bash
# Automated QEMU smoke test: inject shell commands (staggered so the guest
# UART is ready before each line), assert on output.
set -u
cd "$(dirname "$0")"

OUT=$(mktemp)
{
    sleep 1
    printf 'help\n';        sleep 0.5
    printf 'echo hello-from-minios\n'; sleep 0.5
    printf 'ps\n';          sleep 0.5
    printf 'uptime\n';      sleep 2
} | timeout 12 qemu-system-arm -M lm3s6965evb -nographic -monitor none \
      -serial stdio -kernel build/minios.elf >"$OUT" 2>&1

rc=$?
fail() { echo "FAIL: $1"; echo "--- output ---"; cat "$OUT"; rm -f "$OUT"; exit 1; }

grep -q "minios shell"      "$OUT" || fail "no shell banner"
grep -q "commands: help"    "$OUT" || fail "help command did not work"
grep -q "hello-from-minios" "$OUT" || fail "echo command did not work"
grep -q "idle.*runnable"    "$OUT" || fail "ps: idle not listed as runnable"
grep -q "heartbeat.*sleeping" "$OUT" || fail "ps: heartbeat state wrong"
grep -q "uptime: "          "$OUT" || fail "uptime command did not work"
[ "$(grep -c heartbeat "$OUT")" -ge 3 ] || fail "heartbeat did not repeat (scheduler/sleep broken)"
[ "$rc" -eq 124 ] || [ "$rc" -eq 0 ] || fail "qemu exited abnormally (rc=$rc)"

echo "minios QEMU test passed"
rm -f "$OUT"
