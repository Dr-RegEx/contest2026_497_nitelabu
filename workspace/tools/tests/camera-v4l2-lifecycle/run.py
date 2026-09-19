#!/usr/bin/env python3
"""Production DVP worker + V4L2 STREAMOFF concurrency, no hardware success claim."""
import ast
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
base = ast.parse((root / 'tools/tests/camera-dvp-ownership/run.py').read_text())
stubs = next(ast.literal_eval(n.value) for n in base.body if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'stubs' for t in n.targets))
stubs = stubs.replace("#define TIMESPEC_TO_TIMEVAL", "#undef TIMESPEC_TO_TIMEVAL\n#define TIMESPEC_TO_TIMEVAL")
stubs = stubs.replace('#define SP_UNLOCKED 0', '#define SP_UNLOCKED PTHREAD_MUTEX_INITIALIZER')
stubs = stubs.replace('typedef int mutex_t;', 'typedef pthread_mutex_t mutex_t;')
stubs = stubs.replace('(void)m; return 0;', 'return pthread_mutex_trylock(m);', 1)
stubs = stubs.replace('(void)m; return 0;', 'return pthread_mutex_unlock(m);', 1)
stubs = stubs.replace('return 1;', 'return (int)syscall(SYS_gettid);')
stubs = stubs.replace('typedef int spinlock_t;', 'typedef pthread_mutex_t spinlock_t;')
stubs = stubs.replace('(void)l; return 0;', 'pthread_mutex_lock(l); return 0;')
stubs = stubs.replace('(void)l; (void)f;', 'pthread_mutex_unlock(l); (void)f;')
stubs = '#define _GNU_SOURCE\n#include <pthread.h>\n#include <unistd.h>\n#include <sys/syscall.h>\n#include <stdatomic.h>\n#include <stdlib.h>\n' + stubs
stubs += r'''
#define CONFIG_ESP32S31_CAMERA_DVP_HRES 4
#define CONFIG_ESP32S31_CAMERA_DVP_VRES 2
static int nxmutex_lock(mutex_t *m) { return pthread_mutex_lock(m); }
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
static bool hold_copy, copy_entered, release_copy;
static void *checked_copy(void *d, const void *s, size_t n)
{
  pthread_mutex_lock(&gate);
  if (hold_copy) {
    copy_entered = true; pthread_cond_broadcast(&condition);
    while (!release_copy) pthread_cond_wait(&condition, &gate);
  }
  pthread_mutex_unlock(&gate);
  return memcpy(d, s, n);
}
#define memcpy checked_copy
'''
text = (root / 'openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_camera_dvp.c').read_text()
source = text[text.index('struct esp32s31_camera_dvp_s'):text.index('static int esp32s31_camera_result')]
source += text[text.index('int esp32s31_camera_dvp_stop(void)'):]
source += text[text.index('void esp32s31_camera_dvp_uninitialize(void)'):text.index('int esp32s31_camera_dvp_initialize(void)')]
upper = (root / 'openvela-dev/nuttx/drivers/video/v4l2_cap.c').read_text()
upper = upper[upper.index('static int capture_streamoff(FAR struct file *filep,', upper.index('static int capture_streamon(FAR struct file *filep,') + 1):]
# Find definition rather than forward declaration.
if upper.index('{') > upper.index(';'):
    upper = upper[upper.index('static int capture_streamoff(FAR struct file *filep,', 1):]
upper = upper[:upper.index('static int capture_do_halfpush')]
full_upper = (root / 'openvela-dev/nuttx/drivers/video/v4l2_cap.c').read_text()
def definition(name):
    start = full_upper.rindex('static int ' + name + '(')
    brace = full_upper.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (full_upper[end] == '{') - (full_upper[end] == '}')
        end += 1
    return full_upper[start:end] + '\n'
upper += definition('capture_reqbufs') + definition('capture_close')
shim = r'''
#define OK 0
#define CAUSE_CAPTURE_STOP 1
enum v4l2_buf_type { V4L2_BUF_TYPE_VIDEO_CAPTURE };
enum capture_state_e { CAPTURE_STATE_STREAMOFF, CAPTURE_STATE_CAPTURE };
struct imgdata_s;
struct ops { void (*free)(struct imgdata_s *, void *); void *(*alloc)(struct imgdata_s *, int, size_t); };
struct imgdata_s { struct ops *ops; };
struct v4l2_requestbuffers { unsigned count; enum v4l2_buf_type type; int mode, memory; };
typedef struct { enum capture_state_e state; int bufinf; void *bufheap; int fmt[1]; } capture_type_inf_t;
typedef struct { mutex_t mutex; capture_type_inf_t capture; struct imgdata_s *imgdata; int open_num; bool unlinked; } capture_mng_t;
struct inode { void *i_private; };
struct file { struct inode *f_inode; };
#define kmm_free(p) ((void)(p))
#define nxmutex_destroy(m) pthread_mutex_destroy(m)
#define V4L2_REQBUFS_COUNT_MAX 8
#define V4L2_MEMORY_MMAP 1
#define CAPTURE_FMT_MAIN 0
static unsigned reallocations, cleanups;
static void video_framebuff_change_mode(int *b, int m) { (void)b; (void)m; }
static int video_framebuff_realloc_container(int *b, unsigned n) { (void)b; (void)n; reallocations++; return 0; }
static size_t get_bufsize(int *f) { (void)f; return 16; }
static void kumm_free(void *p) { free(p); }
static void *kumm_memalign(int a, size_t n) { (void)a; return malloc(n); }
static void cleanup_resources(capture_mng_t *c) { (void)c; cleanups++; assert(!g_camera.copy_running); assert(esp32s31_camera_dvp_set_target(NULL,0,NULL,NULL)==0); }
#define IMGSENSOR_UNINIT(s) esp32s31_camera_dvp_stop()
#define IMGDATA_UNINIT(d) esp32s31_camera_dvp_set_capture_lock(NULL)
static capture_type_inf_t *get_capture_type_inf(capture_mng_t *c, enum v4l2_buf_type t)
{ (void)t; return &c->capture; }
static enum capture_state_e estimate_next_capture_state(capture_mng_t *c, int cause)
{ (void)c; (void)cause; return CAPTURE_STATE_STREAMOFF; }
static void change_capture_state(capture_mng_t *c, enum capture_state_e state)
{ assert(esp32s31_camera_dvp_set_target(NULL, 0, NULL, NULL) == 0); c->capture.state = state; }
'''
tests = r'''
static unsigned calls;
static bool callback_rotate;
static unsigned char target[16], next[16], frames[2][16];
static int complete(uint8_t err, uint32_t n, const struct timeval *tv, void *a)
{
  (void)tv; (void)a; assert(!err && n == 16); calls++;
  if (callback_rotate) {
    assert(esp32s31_camera_dvp_set_target(next, 16, complete, NULL) == 0);
    assert(esp32s31_camera_dvp_stop() == 0);
    assert(esp32s31_camera_dvp_set_target(NULL, 0, NULL, NULL) == 0);
  }
  return 0;
}
static void enqueue(void)
{
  esp_cam_ctlr_trans_t t = {0};
  esp32s31_camera_get_buffer(NULL, &t, &g_camera);
  assert(t.buffer); memset(t.buffer, 0x61, 16); t.received_size = 16;
  assert(esp32s31_camera_frame_done(NULL, &t, &g_camera));
}
static void *drain(void *arg)
{ (void)arg; void (*fn)(void *) = queued; queued = NULL; assert(fn); fn(queued_arg); return NULL; }
static atomic_bool stop_entered, stop_finished;
static struct file file;
static int lifecycle_operation;
static void *streamoff(void *arg)
{ (void)arg; enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  atomic_store(&stop_entered, true); if (lifecycle_operation == 0) assert(capture_streamoff(&file, &t) == 0);
  else if (lifecycle_operation == 1) { struct v4l2_requestbuffers r = {.count=2}; assert(capture_reqbufs(&file, &r) == -EPERM); }
  else assert(capture_close(&file) == 0);
  atomic_store(&stop_finished, true); return NULL; }
int main(void)
{
  capture_mng_t cmng = { .mutex = PTHREAD_MUTEX_INITIALIZER, .capture.state = CAPTURE_STATE_CAPTURE };
  struct inode inode = { &cmng }; file.f_inode = &inode;
  g_camera.controller = &g_camera; g_camera.frame[0] = frames[0]; g_camera.frame[1] = frames[1];
  g_camera.frame_size = 16; g_camera.started = true;
  esp32s31_camera_dvp_set_capture_lock(&cmng.mutex);
  assert(esp32s31_camera_dvp_set_target(target, 16, complete, NULL) == 0);
  /* Actual STREAMOFF waits for memcpy and completion; external clear cannot race. */
  hold_copy = true; enqueue(); pthread_t worker, stopper;
  assert(pthread_create(&worker, NULL, drain, NULL) == 0);
  pthread_mutex_lock(&gate); while (!copy_entered) pthread_cond_wait(&condition, &gate); pthread_mutex_unlock(&gate);
  assert(esp32s31_camera_dvp_set_target(NULL, 0, NULL, NULL) == -EBUSY);
  assert(pthread_create(&stopper, NULL, streamoff, NULL) == 0);
  while (!atomic_load(&stop_entered)) sched_yield();
  assert(!atomic_load(&stop_finished));
  pthread_mutex_lock(&gate); release_copy = true; pthread_cond_broadcast(&condition); pthread_mutex_unlock(&gate);
  pthread_join(worker, NULL); pthread_join(stopper, NULL);
  assert(calls == 1 && target[0] == 0x61 && !g_camera.target);
  hold_copy = false;
  struct v4l2_requestbuffers resize = {.count=2};
  assert(capture_reqbufs(&file, &resize) == 0 && reallocations == 1);
  /* Actual REQBUFS and CLOSE share the same mutex, cannot free during copy. */
  for (int op = 1; op <= 2; op++) {
    lifecycle_operation = op; cmng.capture.state = CAPTURE_STATE_CAPTURE; cmng.open_num = 1; cmng.unlinked = op == 2;
    g_camera.started = true;
    assert(esp32s31_camera_dvp_set_target(target,16,complete,NULL)==0);
    hold_copy = true; copy_entered = false; release_copy = false;
    atomic_store(&stop_entered,false); atomic_store(&stop_finished,false);
    enqueue(); pthread_create(&worker,NULL,drain,NULL);
    pthread_mutex_lock(&gate); while (!copy_entered) pthread_cond_wait(&condition,&gate); pthread_mutex_unlock(&gate);
    pthread_create(&stopper,NULL,streamoff,NULL);
    while (!atomic_load(&stop_entered)) sched_yield();
    assert(!atomic_load(&stop_finished));
    pthread_mutex_lock(&gate); release_copy = true; pthread_cond_broadcast(&condition); pthread_mutex_unlock(&gate);
    pthread_join(worker,NULL); pthread_join(stopper,NULL);
    hold_copy = false;
  }
  assert(calls == 3 && cleanups == 1 && reallocations == 1);
  assert(inode.i_private == NULL);
  assert(pthread_mutex_init(&cmng.mutex, NULL) == 0);
  inode.i_private = &cmng;
  calls = 1; g_camera.started = true;
  esp32s31_camera_dvp_set_capture_lock(&cmng.mutex);
  /* Busy upper lifecycle mutex: worker drops instead of blocking a close drain. */
  assert(esp32s31_camera_dvp_set_target(target, 16, complete, NULL) == 0);
  pthread_mutex_lock(&cmng.mutex); enqueue(); pthread_create(&worker, NULL, drain, NULL); pthread_join(worker, NULL);
  assert(calls == 1 && !g_camera.copy_busy); pthread_mutex_unlock(&cmng.mutex);
  /* Old queued frame cannot enter a new stream / replacement buffer. */
  enqueue(); assert(esp32s31_camera_dvp_set_target(NULL, 0, NULL, NULL) == 0);
  assert(esp32s31_camera_dvp_set_target(next, 16, complete, NULL) == 0); drain(NULL);
  assert(calls == 1 && next[0] == 0);
  /* Real callback may rotate the buffer and stop without deadlocking itself. */
  callback_rotate = true; enqueue(); drain(NULL); assert(calls == 2 && !g_camera.target);
  /* Detach cancels pending worker before mutex destruction. */
  callback_rotate = false;
  assert(esp32s31_camera_dvp_set_target(target, 16, complete, NULL) == 0); enqueue();
  g_camera.started = true; stop_result = -1; assert(esp32s31_camera_dvp_stop() == -EIO);
  pthread_mutex_lock(&cmng.mutex); esp32s31_camera_dvp_set_capture_lock(NULL); pthread_mutex_unlock(&cmng.mutex);
  assert(!queued && !g_camera.capture_lock && !g_camera.copy_busy);
  assert(pthread_mutex_destroy(&cmng.mutex) == 0);
  /* HAL still running after stop error cannot queue into destroyed upper. */
  esp_cam_ctlr_trans_t late = {0};
  esp32s31_camera_get_buffer(NULL, &late, &g_camera);
  assert(late.buffer); late.received_size = 16;
  assert(!esp32s31_camera_frame_done(NULL, &late, &g_camera) && !queued);
  esp32s31_camera_dvp_set_capture_lock(NULL);
  stop_result = 0;
  esp32s31_camera_dvp_uninitialize();
  assert(freed == 2);
  return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='camera-lifecycle-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(stubs + source + shim + upper + tests)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pthread', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS production DVP + V4L2 STREAMOFF/REQBUFS/CLOSE: concurrent copy/stop, external EBUSY, upper lock busy drop, generation rejection, callback rotation/stop, mutex detach')
