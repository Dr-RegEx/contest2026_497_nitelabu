#!/usr/bin/env python3
"""Host regression of production DVP frame ownership callbacks (no camera I/O)."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = root / 'openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_camera_dvp.c'
text = source.read_text()
callbacks = text[text.index('struct esp32s31_camera_dvp_s'):text.index('static int esp32s31_camera_result')]
callbacks += text[text.index('int esp32s31_camera_dvp_stop(void)'):]
callbacks += text[text.index('void esp32s31_camera_dvp_uninitialize(void)'):text.index('int esp32s31_camera_dvp_set_target')]
stubs = r'''
#include <assert.h>
#include <errno.h>
#include <syslog.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#define FAR
#define IRAM_ATTR
#define HPWORK 0
#define SP_UNLOCKED 0
#define TIMESPEC_TO_TIMEVAL(tv, ts) do { (tv)->tv_sec=(ts)->tv_sec; (tv)->tv_usec=0; } while (0)
typedef void *esp_cam_ctlr_handle_t;
typedef int (*esp32s31_camera_dvp_capture_t)(uint8_t, uint32_t, const struct timeval *, void *);
typedef struct { void *buffer; size_t buflen; size_t received_size; } esp_cam_ctlr_trans_t;
typedef int mutex_t;
static int nxmutex_trylock(mutex_t *m) { (void)m; return 0; }
static int nxmutex_unlock(mutex_t *m) { (void)m; return 0; }
static int nxsched_gettid(void) { return 1; }
typedef int spinlock_t;
typedef int irqstate_t;
struct work_s { int unused; };
static void (*queued)(void *);
static void *queued_arg;
static bool reject_queue;
typedef int esp_err_t;
#define ESP_OK 0
static int stop_result, disable_result, delete_result;
static unsigned freed;
static int esp_cam_ctlr_stop(void *p) { (void)p; return stop_result; }
static int esp_cam_ctlr_disable(void *p) { (void)p; return disable_result; }
static int esp_cam_ctlr_del(void *p) { (void)p; return delete_result; }
static void heap_caps_free(void *p) { assert(p); freed++; }
static int work_cancel_sync(int q, struct work_s *w);
static int esp32s31_camera_result(int r) { return r == 0 ? 0 : -EIO; }
static int work_cancel(int q, struct work_s *w)
{
  (void)q; (void)w;
  if (!queued) return -ENOENT;
  queued = NULL; return 0;
}
static int work_cancel_sync(int q, struct work_s *w)
{
  return work_cancel(q, w);
}
static irqstate_t spin_lock_irqsave(spinlock_t *l) { (void)l; return 0; }
static void spin_unlock_irqrestore(spinlock_t *l, irqstate_t f) { (void)l; (void)f; }
static void clock_systime_timespec(struct timespec *ts) { memset(ts, 0, sizeof(*ts)); }
static int work_queue(int q, struct work_s *w, void (*fn)(void *), void *a, int delay)
{
  (void)q; (void)w; (void)delay;
  if (reject_queue) return -16;
  assert(queued == NULL);
  queued = fn; queued_arg = a; return 0;
}
'''
tests = r'''
static unsigned completions;
static unsigned char delivered[16];
static int complete(uint8_t err, uint32_t n, const struct timeval *tv, void *arg)
{
  (void)tv; (void)arg;
  assert(err == 0 && n == sizeof(delivered));
  completions++;
  return 0;
}
static esp_cam_ctlr_trans_t acquire(void)
{
  esp_cam_ctlr_trans_t t = {0};
  esp32s31_camera_get_buffer(NULL, &t, &g_camera);
  return t;
}
static void drain(void)
{
  void (*fn)(void *) = queued;
  void *a = queued_arg;
  assert(fn); queued = NULL; fn(a);
}
int main(void)
{
  unsigned char a[16], b[16];
  g_camera.frame[0] = a; g_camera.frame[1] = b;
  g_camera.frame_size = sizeof(a);
  g_camera.target = delivered; g_camera.target_size = sizeof(delivered);
  g_camera.capture_cb = complete;

  /* Match the SDK ordering: next DMA buffer before previous completion. */
  esp_cam_ctlr_trans_t first = acquire();
  memset(first.buffer, 0x31, first.buflen);
  first.received_size = first.buflen;
  esp_cam_ctlr_trans_t second = acquire();
  assert(first.buffer != second.buffer && second.buffer);
  assert(esp32s31_camera_frame_done(NULL, &first, &g_camera));
  memset(second.buffer, 0x62, second.buflen);
  second.received_size = second.buflen;
  esp_cam_ctlr_trans_t backup = acquire();
  assert(backup.buffer == NULL); /* Controller uses its private fallback. */
  assert(!esp32s31_camera_frame_done(NULL, &second, &g_camera));
  drain();
  assert(completions == 1);
  for (size_t i = 0; i < sizeof(delivered); i++) assert(delivered[i] == 0x31);

  /* No buffer can remain pinned after a dropped frame / busy worker. */
  assert(g_camera.frame_state[0] == FRAME_FREE);
  assert(g_camera.frame_state[1] == FRAME_FREE);
  assert(!g_camera.copy_busy);

  first = acquire(); first.received_size = sizeof(a) - 1;
  assert(!esp32s31_camera_frame_done(NULL, &first, &g_camera));
  assert(queued == NULL && completions == 1); /* No short-frame success. */
  first = acquire(); first.received_size = sizeof(a) + 1;
  assert(!esp32s31_camera_frame_done(NULL, &first, &g_camera));
  first = acquire(); first.received_size = sizeof(a);
  reject_queue = true;
  assert(!esp32s31_camera_frame_done(NULL, &first, &g_camera));
  assert(!g_camera.copy_busy && g_camera.frame_state[0] == FRAME_FREE);
  reject_queue = false;

  /* Unknown/backup pointers must not corrupt ownership or publish frames. */
  unsigned char other[16]; first.buffer = other;
  assert(!esp32s31_camera_frame_done(NULL, &first, &g_camera));
  for (int round = 0; round < 100; round++)
    {
      first = acquire(); assert(first.buffer);
      memset(first.buffer, round, first.buflen);
      first.received_size = first.buflen;
      assert(esp32s31_camera_frame_done(NULL, &first, &g_camera));
      drain();
      for (size_t i = 0; i < sizeof(delivered); i++) assert(delivered[i] == round);
    }
  assert(completions == 101);
  /* Stopping cancels a queued copy and releases all DMA-owned frames. */
  g_camera.controller = &g_camera;
  g_camera.started = true;
  first = acquire(); first.received_size = first.buflen;
  second = acquire();
  assert(esp32s31_camera_frame_done(NULL, &first, &g_camera));
  assert(esp32s31_camera_dvp_stop() == 0);
  assert(!queued && !g_camera.copy_busy);
  assert(g_camera.frame_state[0] == FRAME_FREE);
  assert(g_camera.frame_state[1] == FRAME_FREE);
  /* A worker already running must retain ownership across stop. */
  g_camera.started = true;
  g_camera.frame_state[0] = FRAME_COPY;
  g_camera.copy_busy = true;
  assert(esp32s31_camera_dvp_stop() == 0);
  assert(g_camera.frame_state[0] == FRAME_COPY && g_camera.copy_busy);
  /* A failed HAL teardown must keep resources alive for retry. */
  g_camera.started = true; g_camera.enabled = true;
  stop_result = -1;
  esp32s31_camera_dvp_uninitialize();
  assert(g_camera.started && g_camera.enabled && freed == 0);
  stop_result = 0; disable_result = -1;
  esp32s31_camera_dvp_uninitialize();
  assert(!g_camera.started && g_camera.enabled && freed == 0);
  disable_result = 0; delete_result = -1;
  esp32s31_camera_dvp_uninitialize();
  assert(!g_camera.enabled && g_camera.controller && freed == 0);
  delete_result = 0;
  esp32s31_camera_dvp_uninitialize();
  assert(g_camera.controller == NULL && freed == 2);
  esp32s31_camera_dvp_uninitialize();
  assert(freed == 2);
  return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='camera-ownership-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(stubs + callbacks + tests)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: SDK callback ordering, pending DMA isolation, busy drop, short/oversize rejection, queue failure, stop ownership, teardown failures/retry, 100 frames')
