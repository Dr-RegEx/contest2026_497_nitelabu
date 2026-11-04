/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_monstat.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Observation surface for the M-mode monitor's interrupt deferral protocol
 * and for address environment lifetime.  Both answer questions that no NSH
 * command can: a replayed CLIC input looks exactly like a directly forwarded
 * one from S-mode, and `free` deltas cannot separate a leaked address
 * environment from allocator fragmentation.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <nuttx/fs/fs.h>
#include <nuttx/mutex.h>
#include <nuttx/sched.h>

#include "esp32s31_monstat.h"

#ifdef CONFIG_ARCH_USE_S_MODE

#ifdef CONFIG_ARCH_ADDRENV
#  include "addrenv.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Must match the layout allocated by esp32s31_supervisor.S. */

#define MONSTAT_CORES       2
#define MONSTAT_WORDS       8
#define MONSTAT_DEFER       0
#define MONSTAT_REPLAY      1
#define MONSTAT_ACK         2
#define MONSTAT_DROP        3
#define MONSTAT_SREPLAY     4

/* The text is built in a shared buffer under a mutex rather than on the
 * stack of a user-mode reader, which is too small for the full report.
 */

#define MONSTAT_TEXT_LEN    4096

/****************************************************************************
 * External Data
 ****************************************************************************/

extern volatile uint32_t
  g_esp32s31_machine_monstat[MONSTAT_CORES][MONSTAT_WORDS];
extern volatile uint32_t g_esp32s31_machine_forward_mask[MONSTAT_CORES];
extern volatile uint32_t g_esp32s31_machine_monlat[MONSTAT_CORES][16];
#if defined(CONFIG_ESPRESSIF_WIFI) && defined(CONFIG_NET_STATISTICS)
extern uint32_t g_s31_rx_callback_diag;
extern uint32_t g_s31_tcp_synack_diag[6];
size_t esp32s31_wifi_queue_diag_format(char *text, size_t textlen);
extern uint32_t g_s31_arp_diag[3][10];
extern uint32_t g_s31_tx_done_diag[4];
extern uint32_t g_s31_tx_shape_diag[5];
extern uint32_t g_s31_arp_request_diag[6];
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static ssize_t esp32s31_monstat_read(FAR struct file *filep,
                                     FAR char *buffer, size_t buflen);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char g_esp32s31_monstat_text[MONSTAT_TEXT_LEN];
static mutex_t g_esp32s31_monstat_lock = NXMUTEX_INITIALIZER;

/* Temporary network-lock diagnosis.  Never print stack contents or names. */

static struct
{
  pid_t pid;
  void *waitobj;
} g_s31_waiters[12];
static unsigned int g_s31_waiter_count;

static const struct file_operations g_esp32s31_monstat_ops =
{
  NULL,                     /* open */
  NULL,                     /* close */
  esp32s31_monstat_read,    /* read */
  NULL,                     /* write */
  NULL,                     /* seek */
  NULL,                     /* ioctl */
  NULL,                     /* mmap */
  NULL,                     /* truncate */
  NULL,                     /* poll */
  NULL,                     /* readv */
  NULL                      /* writev */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void esp32s31_waiter_collect(struct tcb_s *tcb, void *arg)
{
  UNUSED(arg);

  if (tcb->task_state == TSTATE_WAIT_SEM &&
      g_s31_waiter_count < 12)
    {
      g_s31_waiters[g_s31_waiter_count].pid = tcb->pid;
      g_s31_waiters[g_s31_waiter_count].waitobj = tcb->waitobj;
      g_s31_waiter_count++;
    }
}

static size_t esp32s31_monstat_format(FAR char *text, size_t textlen)
{
  size_t len = 0;
  int cpu;

  for (cpu = 0; cpu < MONSTAT_CORES && len < textlen; cpu++)
    {
      len += snprintf(text + len, textlen - len,
                      "MONIRQ: cpu=%d defer=%" PRIu32 " replay=%" PRIu32
                      " ack=%" PRIu32 " drop=%" PRIu32
                      " pending=0x%" PRIx32 " sreplay=%" PRIu32 "\n",
                      cpu,
                      g_esp32s31_machine_monstat[cpu][MONSTAT_DEFER],
                      g_esp32s31_machine_monstat[cpu][MONSTAT_REPLAY],
                      g_esp32s31_machine_monstat[cpu][MONSTAT_ACK],
                      g_esp32s31_machine_monstat[cpu][MONSTAT_DROP],
                      g_esp32s31_machine_forward_mask[cpu],
                      g_esp32s31_machine_monstat[cpu][MONSTAT_SREPLAY]);

      if (len < textlen)
        {
          len += snprintf(text + len, textlen - len,
                          "MONLAT: scope=monitor_to_wifi_cb units=cycles "
                          "cpu=%d input=1 last=%" PRIu32
                          " max=%" PRIu32 " samples=%" PRIu32
                          " over32=%" PRIu32 " over100=%" PRIu32
                          " over1000=%" PRIu32 " sum=%" PRIu32
                          " direct=%" PRIu32 " directmax=%" PRIu32
                          " deferred=%" PRIu32 " deferredmax=%" PRIu32 "\n",
                          cpu, g_esp32s31_machine_monlat[cpu][1],
                          g_esp32s31_machine_monlat[cpu][2],
                          g_esp32s31_machine_monlat[cpu][3],
                          g_esp32s31_machine_monlat[cpu][4],
                          g_esp32s31_machine_monlat[cpu][5],
                          g_esp32s31_machine_monlat[cpu][6],
                          g_esp32s31_machine_monlat[cpu][7],
                          g_esp32s31_machine_monlat[cpu][11],
                          g_esp32s31_machine_monlat[cpu][9],
                          g_esp32s31_machine_monlat[cpu][12],
                          g_esp32s31_machine_monlat[cpu][10]);
        }
    }

#ifdef CONFIG_ARCH_ADDRENV
  if (len < textlen)
    {
      len += snprintf(text + len, textlen - len,
                      "ADDRENV: created=%" PRIu32 " fail=%" PRIu32
                      " destroyed=%" PRIu32 " live=%" PRIu32
                      " livemax=%" PRIu32 " pages=%" PRIu32
                      " pgt=%" PRIu32 "\n",
                      g_riscv_addrenv_stats.created,
                      g_riscv_addrenv_stats.create_fail,
                      g_riscv_addrenv_stats.destroyed,
                      g_riscv_addrenv_stats.live,
                      g_riscv_addrenv_stats.live_max,
                      g_riscv_addrenv_stats.pages_freed,
                      g_riscv_addrenv_stats.pgt_freed);
    }
#endif

#if defined(CONFIG_ESPRESSIF_WIFI) && defined(CONFIG_NET_STATISTICS)
  if (len < textlen)
    {
      len += snprintf(text + len, textlen - len,
                      "WIFIRX: callbacks=%" PRIu32 "\n",
                      __atomic_load_n(&g_s31_rx_callback_diag,
                                      __ATOMIC_RELAXED));
    }

  if (len < textlen)
    {
      len += esp32s31_wifi_queue_diag_format(text + len, textlen - len);
    }

  if (len < textlen)
    {
      uint32_t diag[6];
      for (unsigned int i = 0; i < 6; i++)
        {
          diag[i] = __atomic_load_n(&g_s31_tcp_synack_diag[i],
                                    __ATOMIC_RELAXED);
        }

      len += snprintf(text + len, textlen - len,
                      "TCPSYNACK: seen=%" PRIu32 " queued=%" PRIu32
                      " nomem=%" PRIu32 " error=%" PRIu32
                      " badip=%" PRIu32 " badtcp=%" PRIu32 "\n",
                      diag[0], diag[1], diag[2], diag[3], diag[4], diag[5]);
    }

  if (len < textlen)
    {
      uint32_t diag[4];
      for (unsigned int i = 0; i < 4; i++)
        {
          diag[i] = __atomic_load_n(&g_s31_tx_done_diag[i],
                                    __ATOMIC_RELAXED);
        }

      len += snprintf(text + len, textlen - len,
                      "TXDONE: total=%" PRIu32 " fail=%" PRIu32
                      " invalid=%" PRIu32 " nonarp=%" PRIu32 "\n",
                      diag[0], diag[1], diag[2], diag[3]);
    }

  if (len < textlen)
    {
      uint32_t diag[5];
      for (unsigned int i = 0; i < 5; i++)
        {
          diag[i] = __atomic_load_n(&g_s31_tx_shape_diag[i],
                                    __ATOMIC_RELAXED);
        }

      len += snprintf(text + len, textlen - len,
                      "TXSHAPE: snap=%" PRIu32 " arphdr=%" PRIu32
                      " arpfail=%" PRIu32 " offset=%" PRIu32
                      " len=%" PRIu32 "\n",
                      diag[0], diag[1], diag[2], diag[3], diag[4]);
    }

  if (len < textlen)
    {
      uint32_t diag[6];
      for (unsigned int i = 0; i < 6; i++)
        {
          diag[i] = __atomic_load_n(&g_s31_arp_request_diag[i],
                                    __ATOMIC_RELAXED);
        }

      len += snprintf(text + len, textlen - len,
                      "ARPREQ: seen=%" PRIu32 " baddst=%" PRIu32
                      " badsrc=%" PRIu32 " badsha=%" PRIu32
                      " badspa=%" PRIu32 " badtha=%" PRIu32 "\n",
                      diag[0], diag[1], diag[2], diag[3], diag[4], diag[5]);
    }

  for (unsigned int dir = 0; dir < 3 && len < textlen; dir++)
    {
      uint32_t diag[10];
      for (unsigned int i = 0; i < 10; i++)
        {
          diag[i] = __atomic_load_n(&g_s31_arp_diag[dir][i],
                                    __ATOMIC_RELAXED);
        }

      len += snprintf(text + len, textlen - len,
                      "ARPDIAG: dir=%u frames=%" PRIu32 " bad=%" PRIu32
                      " req=%" PRIu32 " reply=%" PRIu32 " peer=%" PRIu32
                      " peerreply=%" PRIu32 " lastlen=%" PRIu32
                      " srcdiff=%" PRIu32 " ok=%" PRIu32
                      " drop=%" PRIu32 "\n", dir, diag[0], diag[1], diag[2],
                      diag[3], diag[4], diag[5], diag[6], diag[7],
                      diag[8], diag[9]);
    }
#endif

  g_s31_waiter_count = 0;
  nxsched_foreach(esp32s31_waiter_collect, NULL);
  for (unsigned int i = 0; i < g_s31_waiter_count && len < textlen; i++)
    {
      len += snprintf(text + len, textlen - len,
                      "WAIT: pid=%d object=%p\n", g_s31_waiters[i].pid,
                      g_s31_waiters[i].waitobj);

      /* PID 2 is the verified kernel LP worker, not a user address space. */

      if (g_s31_waiters[i].pid == 2 && len < textlen)
        {
          void *trace[16];
          int count = sched_backtrace(2, trace, 16, 0);

          for (int j = 0; j < count && len < textlen; j++)
            {
              len += snprintf(text + len, textlen - len,
                              "LPTRACE: %p\n", trace[j]);
            }
        }
    }

  if (len >= textlen)
    {
      len = textlen - 1;
    }

  return len;
}

static ssize_t esp32s31_monstat_read(FAR struct file *filep,
                                     FAR char *buffer, size_t buflen)
{
  FAR char *text = g_esp32s31_monstat_text;
  size_t len;
  size_t ncopy;
  int ret;

  ret = nxmutex_lock(&g_esp32s31_monstat_lock);
  if (ret < 0)
    {
      return ret;
    }

  len = esp32s31_monstat_format(text, MONSTAT_TEXT_LEN);

  if (filep->f_pos >= (off_t)len)
    {
      nxmutex_unlock(&g_esp32s31_monstat_lock);
      return 0;
    }

  ncopy = len - (size_t)filep->f_pos;
  if (ncopy > buflen)
    {
      ncopy = buflen;
    }

  memcpy(buffer, text + filep->f_pos, ncopy);
  filep->f_pos += (off_t)ncopy;
  nxmutex_unlock(&g_esp32s31_monstat_lock);
  return (ssize_t)ncopy;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int esp32s31_monstat_register_device(void)
{
  return register_driver("/dev/s31stat", &g_esp32s31_monstat_ops, 0444,
                         NULL);
}

#endif /* CONFIG_ARCH_USE_S_MODE */
