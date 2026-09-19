#!/usr/bin/env python3
"""Exercise production slot configuration and bus-width register selection."""
from pathlib import Path
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=root / 's31-reference/deps/esp-hal-3rdparty/components/upper_hal_sdmmc/src/sd_host_sdmmc.c')
args = parser.parse_args()
source = args.source.read_text()

def function(name, static=False):
    start = source.rindex(('static ' if static else '') + 'esp_err_t ' + name + '(')
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

preamble = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_NOT_SUPPORTED 2
#define ESP_RETURN_ON_FALSE(c,e,...) do { if (!(c)) return (e); } while (0)
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define SDMMC_LL_DELAY_PHASE_SUPPORTED 1
#define SOC_SDMMC_UHS_I_SUPPORTED 1
#define SDMMC_LL_SDR104_SUPPORTED 1
#define SDMMC_LL_DDR50_SUPPORTED 1
#define SDMMC_LL_SDR50_SUPPORTED 1
#define CONFIG_IDF_TARGET_ESP32P4 0
#define SOC_SDMMC_DELAY_PHASE_NUM 4
#define SD_HOST_SLOT_STATE_INIT 0
#define SD_HOST_SLOT_STATE_READY 1
#define SD_BUS_WIDTH_1_BIT 1
#define SD_BUS_WIDTH_4_BIT 4
#define SD_BUS_WIDTH_8_BIT 8
#define GPIO_MODE_INPUT_OUTPUT 3
#define __containerof(p,t,m) ((t *)((char *)(p) - offsetof(t,m)))
typedef int esp_err_t;
typedef int *sd_host_slot_handle_t;
typedef struct { int width,freq_hz,sampling_mode,delayphase,delayline; } sd_host_slot_cfg_t;
typedef struct { int d3_io; } sd_host_sdmmc_slot_io_cfg_t;
struct ctlr { int spinlock; struct { void *dev; } hal; };
typedef struct {
  int drv,slot_id; struct ctlr *ctlr; bool use_gpio_matrix;
  sd_host_sdmmc_slot_io_cfg_t io_config;
  struct { int width,active_width,width_state; } width;
  struct { int freq_hz,freq_state; } freq;
  struct { int mode,sampling_mode_state; } sampling_mode;
  struct { int delayphase,delay_phase_state; } delay_phase;
  struct { int delayline,delay_line_state; } delay_line;
} sd_host_sdmmc_slot_t;
static const struct { int d3; } sdmmc_slot_gpio_sig[2]={{3},{4}};
static int critical, hw_width, d3_configs;
#define portENTER_CRITICAL(p) do { (void)(p); assert(!critical); critical++; } while (0)
#define portEXIT_CRITICAL(p) do { (void)(p); assert(critical==1); critical--; } while (0)
static void sdmmc_ll_set_card_width(void*dev,int slot,int width) { assert(dev && slot==0);hw_width=width; }
static void configure_pin(int pin,int sig,int mode,const char*name,bool matrix) {
  (void)matrix;assert(pin==23 && sig==3 && mode==3 && name[0]=='d');d3_configs++;
}
'''
tests = r'''
int main(void) {
  struct ctlr ctlr={.hal={.dev=&ctlr}};
  sd_host_sdmmc_slot_t slot={.ctlr=&ctlr,.width={.width=4,.active_width=1},.io_config={.d3_io=23}};
  sd_host_slot_cfg_t cfg={0};
  assert(sd_host_slot_set_bus_width(&slot)==0 && hw_width==1);
  /* Actual mmcsd_probe calls mmcsd_removed -> widebus(false) first. */
  cfg.width=1;
  assert(sd_host_slot_sdmmc_configure(&slot.drv,&cfg)==0);
  assert(sd_host_slot_set_bus_width(&slot)==0 && hw_width==1 && !d3_configs);
  cfg.width=4;
  assert(sd_host_slot_sdmmc_configure(&slot.drv,&cfg)==0);
  assert(sd_host_slot_set_bus_width(&slot)==0 && hw_width==4 && d3_configs==1);
  cfg.width=1;
  assert(sd_host_slot_sdmmc_configure(&slot.drv,&cfg)==0);
  assert(sd_host_slot_set_bus_width(&slot)==0 && hw_width==1 && d3_configs==1);
  cfg.width=0;cfg.freq_hz=400000;
  assert(sd_host_slot_sdmmc_configure(&slot.drv,&cfg)==0);
  assert(sd_host_slot_set_bus_width(&slot)==0 && hw_width==1 && slot.width.width==4);
  cfg.width=8;cfg.freq_hz=20000000;
  assert(sd_host_slot_sdmmc_configure(&slot.drv,&cfg)==ESP_ERR_INVALID_ARG);
  assert(slot.width.active_width==1 && slot.freq.freq_hz==400000);
  cfg.width=2;
  assert(sd_host_slot_sdmmc_configure(&slot.drv,&cfg)==ESP_ERR_INVALID_ARG);
  slot.width.width=8;cfg.width=4;
  assert(sd_host_slot_sdmmc_configure(&slot.drv,&cfg)==0);
  assert(sd_host_slot_set_bus_width(&slot)==0 && hw_width==4);
  cfg.width=8;
  assert(sd_host_slot_sdmmc_configure(&slot.drv,&cfg)==0);
  assert(sd_host_slot_set_bus_width(&slot)==0 && hw_width==8);
  assert(!critical && slot.width.width==8);
  puts("PASS production SD slot: initial/probe 1-bit, 1->4->1, no-change, invalid width rejection, wired capability retained, 8-bit slot selection");
}
'''
with tempfile.TemporaryDirectory(prefix='s31-sd-width-') as tmp:
    c = Path(tmp) / 'test.c'
    exe = Path(tmp) / 'test'
    c.write_text(preamble + function('sd_host_slot_sdmmc_configure', True) + function('sd_host_slot_set_bus_width') + tests)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
