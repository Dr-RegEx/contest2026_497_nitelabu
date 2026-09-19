#!/usr/bin/env python3
"""Exercise real V4L2 timing ioctls: unsupported sensor and legacy driver."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'openvela-dev/nuttx/drivers/video/v4l2_cap.c').read_text()
def function(name):
    start = source.rindex('static int ' + name + '(')
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]
shim = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#define FAR
#define OK 0
#define DEBUGASSERT assert
#define ASSERT assert
#define CAPTURE_STATE_CAPTURE 1
#define CAPTURE_STATE_STREAMOFF 0
#define V4L2_CAP_TIMEPERFRAME 0x1000
struct v4l2_fract { uint32_t numerator, denominator; };
typedef struct v4l2_fract imgsensor_interval_t;
struct imgsensor_ops_s { bool frame_interval_unsupported; };
struct imgsensor_s { const struct imgsensor_ops_s *ops; };
struct v4l2_streamparm {
  int type;
  union { struct { struct v4l2_fract timeperframe; uint32_t capability; } capture; } parm;
};
typedef struct { int state, nr_fmt, fmt, clip; struct v4l2_fract frame_interval; } capture_type_inf_t;
typedef struct { struct imgsensor_s *imgsensor; void *imgdata; capture_type_inf_t type; } capture_mng_t;
struct inode { void *i_private; };
struct file { struct inode *f_inode; };
static int query_result, queries, validations;
static capture_type_inf_t *get_capture_type_inf(capture_mng_t *m, int t) {
  return t == 1 ? &m->type : NULL;
}
static int query(struct imgsensor_s *s, int t, imgsensor_interval_t *i) {
  (void)s; (void)t; queries++; i->numerator=1; i->denominator=25; return query_result;
}
#define IMGSENSOR_GET_FRAME_INTERVAL query
static int validate_frame_setting(capture_mng_t *m, int t, int n, int fmt, int *clip, struct v4l2_fract *i) {
  (void)m; (void)t; (void)n; (void)fmt; (void)clip; validations++;
  return i->numerator && i->denominator ? 0 : -EINVAL;
}
'''
test = r'''
int main(void) {
 struct imgsensor_ops_s ops = { .frame_interval_unsupported=true };
 struct imgsensor_s sensor = { .ops=&ops };
 capture_mng_t m = { .imgsensor=&sensor, .imgdata=&sensor, .type={.frame_interval={1,30}} };
 struct inode inode = {&m}; struct file file = {&inode};
 struct v4l2_streamparm p = {.type=1};
 for (int state=0; state<=1; state++) {
  m.type.state=state;
  memset(&p.parm, 0xa5, sizeof(p.parm));
  assert(capture_g_parm(&file,&p)==-ENOTTY);
  assert(p.parm.capture.capability==0);
  assert(p.parm.capture.timeperframe.denominator==0);
  p.parm.capture.timeperframe=(struct v4l2_fract){1,60};
  assert(capture_s_parm(&file,&p)==-ENOTTY);
  assert(m.type.frame_interval.denominator==30);
 }
 assert(queries==0 && validations==0);
 ops.frame_interval_unsupported=false; m.type.state=0;
 assert(capture_g_parm(&file,&p)==0 && p.parm.capture.timeperframe.denominator==30);
 assert(p.parm.capture.capability==V4L2_CAP_TIMEPERFRAME);
 p.parm.capture.timeperframe=(struct v4l2_fract){1,60};
 assert(capture_s_parm(&file,&p)==0 && m.type.frame_interval.denominator==60);
 m.type.state=1;
 assert(capture_s_parm(&file,&p)==-EBUSY);
 assert(capture_g_parm(&file,&p)==0 && p.parm.capture.timeperframe.denominator==25);
 query_result=-ENOTTY;
 assert(capture_g_parm(&file,&p)==0 && p.parm.capture.timeperframe.denominator==60);
 assert(capture_g_parm(&file,NULL)==-EINVAL);
 assert(capture_s_parm(&file,NULL)==-EINVAL);
 p.type=2; assert(capture_g_parm(&file,&p)==-EINVAL);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='camera-frame-interval-') as tmp:
    src=Path(tmp)/'test.c'; exe=Path(tmp)/'test'
    src.write_text(shim+function('capture_g_parm')+function('capture_s_parm')+test)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-pie','-no-pie',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: unsupported timing rejects G/S_PARM without fake 30 fps; legacy timing unchanged (ASan/UBSan).')
