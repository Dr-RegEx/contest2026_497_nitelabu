#!/usr/bin/env python3
"""Reproduce unsafe completion return in the actual committed RMT function.

Diagnostic only: observed_bug=True is not an acceptance result. Hardware is
mocked; this checks control flow, not electrical timing or actual cancellation.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path('/home/regex/work/esp32s31-openvela/openvela-dev/nuttx')
sys.path.insert(0, str(root / 'tools'))
from test_esp_hr_timer_lifecycle import definition

source = (root / 'arch/risc-v/src/common/espressif/esp_rmt.c').read_text()
fixture = r'''
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define DEBUGASSERT assert
#define RMT_IS_TX_CHANNEL(c) ((c) == 0)
#define SOC_RMT_CHANNELS_PER_GROUP 4
#define RMT_MEM_ITEM_NUM 48
#define SOC_RMT_SUPPORT_TX_LOOP_COUNT 1
#define OK 0
typedef int rmt_channel_t;
typedef int rmt_idle_level_t;
typedef struct { unsigned level0, duration0; } rmt_item32_t;
typedef struct {
  int tx_sem;
  const rmt_item32_t *tx_data;
  int tx_len_rem, tx_offset, tx_sub_len;
  bool wait_done;
} rmt_obj_t;
static rmt_obj_t object, *p_rmt_obj[] = {&object};
static struct { struct { void *regs; } hal; } g_rmtdev_common;
static int waits, posts, completion_error;
static bool active;
static int nxsem_wait(int *s)
{ assert(s == &object.tx_sem); return waits++ == 0 ? 0 : completion_error; }
static int nxsem_post(int *s)
{ assert(s == &object.tx_sem); posts++; return 0; }
static int rmt_ll_tx_get_mem_blocks(void *r, int c)
{ (void)r; (void)c; return 1; }
static int rmt_ll_tx_get_idle_level(void *r, int c)
{ (void)r; (void)c; return 0; }
static bool rmt_ll_tx_is_loop_enabled(void *r, int c)
{ (void)r; (void)c; return false; }
static void rmt_fill_memory(int c, const rmt_item32_t *p, int n, int o)
{ (void)c; (void)p; (void)n; (void)o; }
static void rmt_set_tx_loop_mode(int c, bool b) { (void)c; (void)b; }
static void rmt_set_tx_thr_intr_en(int c, int b, int n)
{ (void)c; (void)b; (void)n; }
static void rmt_tx_start(int c, bool b) { (void)c; (void)b; active = true; }
'''
fixture += definition(source, 'rmt_write_items')
fixture += r'''
int main(void)
{
  rmt_item32_t frame[96] = {0};
  const int errors[] = {-EINTR, -ECANCELED};
  for (unsigned i = 0; i < 2; i++) {
    object = (rmt_obj_t){0}; active = false; waits = posts = 0;
    completion_error = errors[i];
    int ret = rmt_write_items(0, frame, 96, true);
    bool retained = object.tx_data == frame + 48 && object.tx_len_rem == 48;
    bool observed = ret == 0 && active && retained && posts == 1;
    printf("RMT_COMPLETION_DIAGNOSTIC errno=%d return=%d active=%d "
           "retained_tail=%d ownership_post=%d observed_bug=%d\n",
           completion_error, ret, active, retained, posts, observed);
    assert(observed); /* Deliberately reproduces current bug, not a PASS gate. */
  }
}
'''
with tempfile.TemporaryDirectory(prefix='s31-rmt-completion-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(fixture)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-fno-sanitize-recover=all',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
