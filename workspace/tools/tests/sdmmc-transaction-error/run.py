#!/usr/bin/env python3
"""Fault-inject the HAL transaction function, including its cache cleanup."""
from pathlib import Path
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=root / 's31-reference/deps/esp-hal-3rdparty/components/upper_hal_sdmmc/src/sd_trans_sdmmc.c')
args = parser.parse_args()
source = args.source.read_text()
start = source.index('esp_err_t sd_host_slot_sdmmc_do_transaction(')
body = source[start:]
preamble = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define CONFIG_PM_ENABLE 0
#define ESP_OK 0
#define ESP_FAIL 1
#define ESP_ERR_TIMEOUT 2
#define ESP_ERR_INVALID_ARG 3
#define ESP_ERR_INVALID_SIZE 4
#define ESP_CACHE_MSYNC_FLAG_DIR_C2M 1
#define ESP_CACHE_MSYNC_FLAG_DIR_M2C 2
#define SCF_WAIT_BUSY 0x100
#define MMC_APP_CMD 55
#define SD_SWITCH_VOLTAGE 11
#define SD_HOST_SLOT_STATE_READY 1
#define SDMMC_SENDING_CMD 1
#define SDMMC_SENDING_VOLTAGE_SWITCH 2
#define SDMMC_IDLE 0
#define portMAX_DELAY 0xffffffffu
#define ESP_LOGE(...) ((void)0)
#define ESP_RETURN_ON_FALSE(c,e,...) do { if (!(c)) return (e); } while (0)
#define ESP_GOTO_ON_ERROR(c,l,...) do { ret=(c); if (ret) goto l; } while (0)
#define __containerof(p,t,m) ((t *)((char *)(p) - offsetof(t,m)))
typedef int esp_err_t;
typedef int sdmmc_req_state_t;
typedef int sdmmc_hw_cmd_t;
typedef struct { int placeholder; } sd_host_sdmmc_event_t;
struct controller { int mutex,cur_slot_id; bool is_app_cmd; };
typedef struct { int drv,slot_id; struct controller *ctlr; struct { int freq_state,freq_hz; } freq; } sd_host_sdmmc_slot_t;
typedef int *sd_host_slot_handle_t;
typedef struct { int opcode,flags,error; unsigned arg,timeout_ms; void *data; size_t datalen,buflen,blklen; } sdmmc_command_t;
static int event_error, command_error, sync_error, c2m_error, start_error;
static int cache_line=64, events, cache_reads, cache_writes, locks;
static bool busy_clear=true;
static int xSemaphoreTake(int m,unsigned t) { (void)m;(void)t;locks++;return 1; }
static int xSemaphoreGive(int m) { (void)m;locks--;return 1; }
#define SLOT_OK(name) static int name(sd_host_sdmmc_slot_t*s) { (void)s;return 0; }
SLOT_OK(sd_host_slot_set_card_clk)
SLOT_OK(sd_host_slot_set_bus_width)
SLOT_OK(sd_host_slot_set_bus_sampling_mode)
SLOT_OK(sd_host_set_delay_phase)
SLOT_OK(sd_host_set_delay_line)
static int sd_host_slot_get_real_freq(sd_host_sdmmc_slot_t*s,int*f) { *f=s->freq.freq_hz/1000;return 0; }
static void sd_host_handle_idle_state_events(sd_host_sdmmc_slot_t*s) { (void)s; }
static void handle_voltage_switch_stage1(sd_host_sdmmc_slot_t*s,sdmmc_command_t*c) { (void)s;(void)c; }
static sdmmc_hw_cmd_t make_hw_cmd(sdmmc_command_t*c) { return c->opcode; }
static bool sd_host_check_buffer_alignment(sd_host_sdmmc_slot_t*s,void*p,size_t n) { (void)s;return p && n; }
static int esp_cache_get_line_size_by_addr(void*p) { assert(p);return cache_line; }
static int esp_cache_msync(void*p,size_t n,unsigned flags) {
  assert(p && n==512);
  if (flags==ESP_CACHE_MSYNC_FLAG_DIR_C2M) { cache_writes++;return c2m_error; }
  assert(flags==ESP_CACHE_MSYNC_FLAG_DIR_M2C);cache_reads++;return sync_error;
}
static void sd_host_dma_prepare(sd_host_sdmmc_slot_t*s,void*p,size_t n,size_t b) { (void)s;assert(p && n==512 && b==512); }
static int sd_host_slot_start_command(sd_host_sdmmc_slot_t*s,sdmmc_hw_cmd_t c,unsigned a) { (void)s;(void)c;(void)a;return start_error; }
static int handle_event(sd_host_sdmmc_slot_t*s,sdmmc_command_t*c,sdmmc_req_state_t*state,sd_host_sdmmc_event_t*e) {
  (void)s;(void)e;events++;c->error=command_error;if (!event_error) *state=SDMMC_IDLE;return event_error;
}
static bool wait_for_busy_cleared(sd_host_sdmmc_slot_t*s,unsigned t) { (void)s;(void)t;return busy_clear; }
'''
tests = r'''
int main(void) {
  struct controller ctlr={0};
  sd_host_sdmmc_slot_t slot={.ctlr=&ctlr,.freq={.freq_state=1,.freq_hz=400000}};
  unsigned char buffer[512]={0};
  sdmmc_command_t cmd={.opcode=18,.data=buffer,.datalen=512,.buflen=512,.blklen=512,.timeout_ms=100};
  event_error=ESP_ERR_TIMEOUT;
  assert(sd_host_slot_sdmmc_do_transaction(&slot.drv,&cmd)==ESP_ERR_TIMEOUT);
  assert(cache_reads==1 && cache_writes==1 && events==1 && locks==0);
  event_error=ESP_FAIL;
  assert(sd_host_slot_sdmmc_do_transaction(&slot.drv,&cmd)==ESP_FAIL);
  sync_error=ESP_ERR_INVALID_ARG;
  assert(sd_host_slot_sdmmc_do_transaction(&slot.drv,&cmd)==ESP_FAIL);
  event_error=0;
  assert(sd_host_slot_sdmmc_do_transaction(&slot.drv,&cmd)==ESP_ERR_INVALID_ARG);
  sync_error=0;cmd.flags=SCF_WAIT_BUSY;busy_clear=false;
  assert(sd_host_slot_sdmmc_do_transaction(&slot.drv,&cmd)==ESP_ERR_TIMEOUT);
  busy_clear=true;command_error=ESP_FAIL;
  assert(sd_host_slot_sdmmc_do_transaction(&slot.drv,&cmd)==0 && cmd.error==ESP_FAIL);
  command_error=0;
  assert(sd_host_slot_sdmmc_do_transaction(&slot.drv,&cmd)==0 && cmd.error==0);
  int before=events; c2m_error=ESP_FAIL;
  assert(sd_host_slot_sdmmc_do_transaction(&slot.drv,&cmd)==ESP_FAIL && before==events);
  c2m_error=0;start_error=ESP_ERR_TIMEOUT;
  assert(sd_host_slot_sdmmc_do_transaction(&slot.drv,&cmd)==ESP_ERR_TIMEOUT && before==events);
  start_error=0;cache_line=0;event_error=ESP_ERR_TIMEOUT;
  assert(sd_host_slot_sdmmc_do_transaction(&slot.drv,&cmd)==ESP_ERR_TIMEOUT);
  cmd.data=NULL;cmd.datalen=0;cmd.buflen=0;cache_line=64;
  assert(sd_host_slot_sdmmc_do_transaction(&slot.drv,&cmd)==ESP_ERR_TIMEOUT);
  assert(locks==0);
  puts("PASS actual HAL transaction: event/busy timeout preserved, cache error propagation, command error retained, success/uncached/non-data/early-failure paths");
}
'''
with tempfile.TemporaryDirectory(prefix='s31-sd-error-') as tmp:
    c = Path(tmp) / 'test.c'
    exe = Path(tmp) / 'test'
    c.write_text(preamble + body + tests)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
