"""Run production bridge functions with an instrumented, fake HAL on the host."""
from pathlib import Path
import subprocess,tempfile
src=Path('openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_sdmmc.c').read_text()
body=src[src.index('struct esp32s31_sdmmc_dev_s'):src.index('static struct esp32s31_sdmmc_dev_s g_sdmmc')]
preamble=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#define CONFIG_SDIO_BLOCKSETUP 1
#define CONFIG_ESP32S31_SDMMC_IDF_HOST 1
#define UNUSED(x) (void)(x)
#define OK 0
#define ESP_OK 0
#define ESP_ERR_TIMEOUT 1
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERR_INVALID_CRC 3
#define ESP_ERR_NO_MEM 4
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_DMA 2
#define MALLOC_CAP_8BIT 4
#define portTICK_PERIOD_MS 10
#define MMCSD_RESPONSE_MASK (15<<6)
#define MMCSD_R1_RESPONSE (1<<6)
#define MMCSD_R1B_RESPONSE (2<<6)
#define MMCSD_R2_RESPONSE (3<<6)
#define MMCSD_R3_RESPONSE (4<<6)
#define MMCSD_R4_RESPONSE (5<<6)
#define MMCSD_R5_RESPONSE (6<<6)
#define MMCSD_R6_RESPONSE (7<<6)
#define MMCSD_R7_RESPONSE (8<<6)
#define MMCSD_DATAXFR_MASK (7<<10)
#define MMCSD_NODATAXFR 0
#define MMCSD_WRXFR (4<<10)
#define MMCSD_CMDIDX_MASK 63
#define SCF_RSP_R0 0
#define SCF_RSP_R1 0x1c00
#define SCF_RSP_R1B 0x1d00
#define SCF_RSP_R2 0x1600
#define SCF_RSP_R3 0x1000
#define SCF_RSP_R4 0x1000
#define SCF_RSP_R5 0x1c00
#define SCF_RSP_R6 0x1c00
#define SCF_RSP_R7 0x1c00
#define SCF_CMD_ADTC 0x10
#define SCF_CMD_AC 0
#define SCF_CMD_READ 0x40
#define SDIO_STATUS_PRESENT 1
#define SDIO_CAPS_4BIT 8
#define SDIO_CAPS_DMABEFOREWRITE 4
#define SDIOWAIT_CMDDONE 1
#define SDIOWAIT_RESPONSEDONE 2
#define SDIOWAIT_TRANSFERDONE 4
#define SDIOWAIT_TIMEOUT 8
#define SDIOWAIT_ERROR 16
typedef int esp_err_t;
typedef unsigned sdio_eventset_t;
typedef unsigned sdio_statset_t;
typedef unsigned sdio_capset_t;
struct sdio_dev_s {int unused;};
enum sdio_clock_e {CLOCK_MMC_TRANSFER,CLOCK_SD_TRANSFER_4BIT,CLOCK_SD_TRANSFER_1BIT,CLOCK_SDIO_DISABLED,CLOCK_IDMODE};
typedef struct {unsigned opcode,arg;int flags;size_t datalen,buflen,blklen;void*data;int timeout_ms,error;uint32_t response[4];} sdmmc_command_t;
typedef struct {int slot;int(*init)(void);int(*set_bus_width)(int,int);int(*set_card_clk)(int,uint32_t);int(*do_transaction)(int,sdmmc_command_t*);bool(*check_buffer_alignment)(int,const void*,size_t);} sdmmc_host_t;
typedef struct {int width;} sdmmc_slot_config_t;
static int init_calls,slot_calls,transactions,slot_error,hal_error,cmd_error,allocations;
static bool fail_alloc,reset_done=true;
static size_t cache_alignment=64;
static int SDMMC;
static int64_t timer;
static unsigned expected_ms=1000;
static void *caller_buffer;
static int host_init(void){init_calls++;return 0;}
static int sdmmc_host_init_slot(int s,sdmmc_slot_config_t*c){(void)s;(void)c;slot_calls++;return slot_error;}
static int esp_cache_get_alignment(unsigned c,size_t*a){(void)c;*a=cache_alignment;return 0;}
static void *heap_caps_aligned_alloc(size_t a,size_t n,unsigned c){assert(c==7);if(fail_alloc)return NULL;void*p=NULL;assert(!posix_memalign(&p,a<sizeof(void*)?sizeof(void*):a,n));allocations++;return p;}
static void heap_caps_free(void*p){if(p){allocations--;free(p);}}
static void sdmmc_ll_stop_dma(int*p){assert(p==&SDMMC);}
static bool sdmmc_ll_is_dma_reset_done(int*p){assert(p==&SDMMC);return reset_done;}
static int64_t esp_timer_get_time(void){timer+=1000;return timer;}
static bool aligned(int slot,const void*p,size_t n){(void)slot;return ((uintptr_t)p%64==0)&&(n%64==0);}
static int transaction(int slot,sdmmc_command_t*c){(void)slot;transactions++;assert(c->timeout_ms==(int)expected_ms);c->response[0]=1;c->response[1]=2;c->response[2]=3;c->response[3]=4;c->error=cmd_error;if(c->flags&SCF_CMD_ADTC){assert(c->data&&c->data!=caller_buffer);assert(c->blklen==512||c->blklen==8);assert(c->datalen>=c->blklen);if(c->flags&SCF_CMD_READ)memset(c->data,0xa5,c->datalen);else assert(memcmp(c->data,caller_buffer,c->datalen)==0);}else assert(!c->data&&c->datalen==0);return hal_error;}
'''
tests=r'''
int main(void){struct esp32s31_sdmmc_dev_s p={0};struct sdio_dev_s*d=&p.dev;p.host=(sdmmc_host_t){.init=host_init,.do_transaction=transaction,.check_buffer_alignment=aligned};sdmmc_reset(d);assert(sdmmc_status(d)&SDIO_STATUS_PRESENT);assert(sdmmc_capabilities(d)&SDIO_CAPS_DMABEFOREWRITE);
slot_error=ESP_ERR_TIMEOUT;assert(sdmmc_attach(d)==-ETIMEDOUT);slot_error=0;assert(!sdmmc_attach(d));assert(init_calls==1&&slot_calls==2);
unsigned char raw[1025];caller_buffer=raw+1;memset(raw,0, sizeof(raw));
sdmmc_blocksetup(d,512,2);sdmmc_waitenable(d,SDIOWAIT_TRANSFERDONE|SDIOWAIT_TIMEOUT|SDIOWAIT_ERROR,200);expected_ms=200;assert(!sdmmc_recvsetup(d,raw+1,1024));assert(!sdmmc_sendcmd(d,18|MMCSD_R1_RESPONSE|(1<<10),9));assert(!sdmmc_waitresponse(d,18));assert(raw[0]==0&&raw[1]==0xa5&&raw[1024]==0xa5);assert(sdmmc_eventwait(d)==SDIOWAIT_TRANSFERDONE);assert(allocations==0);assert(sdmmc_eventwait(d)==SDIOWAIT_ERROR);
sdmmc_waitenable(d,SDIOWAIT_TRANSFERDONE|SDIOWAIT_ERROR,100);expected_ms=100;assert(!sdmmc_sendsetup(d,raw+1,1024));assert(!sdmmc_sendcmd(d,25|MMCSD_R1_RESPONSE|(1<<10)|MMCSD_WRXFR,10));assert(sdmmc_eventwait(d)==SDIOWAIT_TRANSFERDONE);assert(!p.buffer);
expected_ms=1000;assert(!sdmmc_sendcmd(d,9|MMCSD_R2_RESPONSE,0));uint32_t r[4];assert(!sdmmc_recv_r2(d,9,r));assert(r[0]==4&&r[1]==3&&r[2]==2&&r[3]==1);
sdmmc_blocksetup(d,512,2);sdmmc_waitenable(d,SDIOWAIT_TRANSFERDONE|SDIOWAIT_ERROR|SDIOWAIT_TIMEOUT,100);expected_ms=100;sdmmc_recvsetup(d,raw+1,1024);memset(raw,0,sizeof(raw));cmd_error=ESP_ERR_INVALID_CRC;assert(sdmmc_sendcmd(d,18|MMCSD_R1_RESPONSE|(1<<10),0)==-EILSEQ);assert(raw[1]==0&&raw[1024]==0);assert(sdmmc_eventwait(d)==SDIOWAIT_ERROR);cmd_error=0;
sdmmc_waitenable(d,SDIOWAIT_TRANSFERDONE|SDIOWAIT_ERROR|SDIOWAIT_TIMEOUT,100);sdmmc_recvsetup(d,raw+1,1024);hal_error=ESP_ERR_TIMEOUT;assert(sdmmc_sendcmd(d,18|(1<<10),0)==-ETIMEDOUT);assert(sdmmc_eventwait(d)==SDIOWAIT_TIMEOUT);hal_error=0;
int before=transactions;sdmmc_recvsetup(d,NULL,1024);assert(sdmmc_sendcmd(d,18|(1<<10),0)==-EINVAL);assert(transactions==before);sdmmc_recvsetup(d,raw+1,512);assert(sdmmc_sendcmd(d,18|(1<<10),0)==-EINVAL);assert(transactions==before);
sdmmc_recvsetup(d,raw+1,1024);fail_alloc=true;assert(sdmmc_sendcmd(d,18|(1<<10),0)==-ENOMEM);assert(transactions==before);fail_alloc=false;
sdmmc_waitenable(d,SDIOWAIT_TRANSFERDONE|SDIOWAIT_ERROR|SDIOWAIT_TIMEOUT,1);expected_ms=10;sdmmc_recvsetup(d,raw+1,1024);reset_done=false;assert(sdmmc_sendcmd(d,18|(1<<10),0)==-ETIMEDOUT);assert(p.retained_buffer&&allocations==1);assert(sdmmc_sendcmd(d,0,0)==-EBUSY);reset_done=true;assert(!sdmmc_sendcmd(d,0,0));assert(allocations==0);
sdmmc_cancel(d);assert(sdmmc_eventwait(d)==SDIOWAIT_ERROR);assert(sdmmc_waitresponse(d,0)==-ECANCELED);
puts("PASS: polling/card probe retry, write ordering capability, aligned private DMA, 2-block RX/TX, R2 order, timeout/CRC/error propagation, bad setup/length/OOM, no phantom completion, stuck-reset buffer quarantine");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(preamble+body+tests)
 subprocess.run(['cc','-D_POSIX_C_SOURCE=200809L','-Wall','-Werror','-Wno-unused-function','-fsanitize=address,undefined',str(p),'-o',str(Path(td)/'test')],check=True)
 subprocess.run([str(Path(td)/'test')],check=True)
