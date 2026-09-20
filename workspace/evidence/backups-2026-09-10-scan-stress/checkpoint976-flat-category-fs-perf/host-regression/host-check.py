from pathlib import Path
import tempfile,subprocess
p=Path(tempfile.mkdtemp(prefix='s31-test01-check-'))
(p/'nuttx').mkdir();(p/'nuttx/config.h').write_text('#define FAR\n')
src=Path('openvela-dev/tests/testcases/vela_fs_test/fs/stability/test01.c').resolve()
(p/'original.c').write_bytes(subprocess.check_output(['git','-C','openvela-dev/tests','show','HEAD:testcases/vela_fs_test/fs/stability/test01.c']))
harness=r'''
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
static atomic_int active;
static int attempts, created_count, joined_count, fail_at;
struct start_info { void *(*fn)(void *); void *arg; };
static void done(void *unused) { atomic_fetch_sub(&active,1); }
static void *proxy(void *v) {
 struct start_info info=*(struct start_info *)v; free(v);
 pthread_cleanup_push(done,NULL);
 atomic_fetch_add(&active,1); info.fn(info.arg);
 pthread_cleanup_pop(1); return NULL;
}
static int host_create(pthread_t *t,const pthread_attr_t *a,void *(*fn)(void *),void *arg) {
 if (++attempts == fail_at) return EAGAIN;
 struct start_info *v=malloc(sizeof(*v)); assert(v); v->fn=fn;v->arg=arg;
 int r=pthread_create(t,a,proxy,v); assert(r==0); ++created_count;
 while (atomic_load(&active) < created_count) sched_yield();
 return r;
}
static int host_join(pthread_t t,void **v) {
 int r=pthread_join(t,v); if (!r) ++joined_count; return r;
}
static int host_stack(pthread_attr_t *a,size_t n) {
 return pthread_attr_setstacksize(a,n<65536?65536:n);
}
static int checked_close(FILE *f);
#define pthread_create host_create
#define pthread_join host_join
#define pthread_attr_setstacksize host_stack
#define fclose checked_close
#define main original_case_main
#include "SOURCE"
#undef main
#undef fclose
#undef pthread_create
#undef pthread_join
#undef pthread_attr_setstacksize
static int checked_close(FILE *f) {
 if(f==test_fp) assert(atomic_load(&active)==0);
 return fclose(f);
}
int main(void) {
 test_time=-1; /* Host-only: exercise shutdown immediately; target source unchanged. */
 do_test(); assert(created_count==3 && joined_count==3 && atomic_load(&active)==0);
 assert(test_flag==0); pthread_mutex_destroy(&mutex);
 attempts=created_count=joined_count=0; fail_at=2;
 do_test(); assert(created_count==1 && joined_count==1 && atomic_load(&active)==0);
 assert(test_flag!=0); pthread_mutex_destroy(&mutex);
 puts("HOST_TEST01_WORKER_DRAIN=PASS normal_and_partial_create_failure"); return 0;
}
'''
for name,source in [('original',p/'original.c'),('patched',src)]:
 (p/'check.c').write_text(harness.replace('SOURCE',str(source)))
 subprocess.run(['gcc','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-pthread','-I',str(p),str(p/'check.c'),'-o',str(p/name)],check=True,capture_output=True)
 work=p/(name+'-data');work.mkdir()
 result=subprocess.run([str(p/name)],cwd=work,capture_output=True,text=True,timeout=45)
 (p/(name+'.log')).write_text(result.stdout+result.stderr)
 if name=='original':
  assert result.returncode!=0 and 'active' in result.stderr,result.stderr
  print('Original source: closes shared FILE while real workers remain active',flush=True)
 else:
  assert result.returncode==0,result.stderr
  print(result.stdout.strip(),flush=True)
print('Host-only evidence:',p,flush=True)
