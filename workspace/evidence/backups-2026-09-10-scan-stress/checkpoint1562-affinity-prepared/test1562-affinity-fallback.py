"""Exercise the real scheduler selection function with an empty ready queue.

This models queue operations; target1563 must verify actual SMP migration.
"""
from pathlib import Path
import subprocess
D = Path(__file__).resolve().parent
root = D.parent.parent

def extract(path):
    text = path.read_text()
    start = text.index('bool nxsched_switch_running(int cpu, bool switch_equal)')
    opening = text.index('{', start)
    level = 1
    end = opening + 1
    while level:
        level += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#define FAR
#define DEBUGASSERT assert
#define CPU_ISSET(c,m) ((*(m) & (1u << (c))) != 0)
#define TCB_FLAG_CPU_LOCKED 1
#define TSTATE_TASK_READYTORUN 1
#define TSTATE_TASK_ASSIGNED 2
#define TSTATE_TASK_RUNNING 3
struct tcb_s { int sched_priority, flags, task_state, cpu; unsigned affinity; struct tcb_s *flink; };
typedef struct tcb_s dq_entry_t;
struct queue_s { dq_entry_t *head; } ready;
struct tcb_s running = {.sched_priority=100,.affinity=2,.cpu=0}, idle;
struct tcb_s *assigned[2] = {&running, &idle};
int locked, delivered;
#define per_cpu_var_smp(x,c) assigned[c]
#define current_task(c) assigned[c]
#define this_cpu() 0
#define list_readytorun() (&ready)
static bool nxsched_islocked_tcb(struct tcb_s *t) {return locked;}
static dq_entry_t *dq_peek(struct queue_s *q) {return q->head;}
static void dq_rem(void *t, struct queue_s *q) {q->head=NULL;}
static bool is_idle_task(struct tcb_s *t) {return t==&idle;}
static void nxsched_add_prioritized(struct tcb_s *t, struct queue_s *q) {q->head=t;}
static void up_update_task(struct tcb_s *t) {}
static bool nxsched_remove_readytorun(struct tcb_s *t) {assert(t==&running);assigned[0]=&idle;return true;}
static bool nxsched_add_readytorun(struct tcb_s *t) {assert(t->affinity==2);delivered++;return false;}
'''
suffix = r'''
int main(void) {
  /* No candidate, unchanged affinity: no switch. */
  running.affinity=1;
  assert(!nxsched_switch_running(0,true));
  /* A scheduler lock still defers switching. */
  running.affinity=2;locked=1;
  assert(!nxsched_switch_running(0,true));
  assert(assigned[0]==&running && delivered==0);
  /* Excluded CPU must fall back to idle even without a ready peer. */
  locked=0;
  if (!nxsched_switch_running(0,true)) return 10;
  assert(assigned[0]==&idle && delivered==1);
  return 0;
}
'''
paths = [('before', D/'affinity1562-before/sched_addreadytorun.c', 10),
         ('after', root/'openvela-dev/nuttx/sched/sched/sched_addreadytorun.c', 0)]
for name, source, expected in paths:
    c = D/f'affinity1562-{name}.c'
    exe = D/f'affinity1562-{name}.elf'
    c.write_text(prefix + extract(source) + suffix)
    subprocess.run(['cc','-std=c11','-Wall','-Wno-unused-parameter','-Wno-unused-function',str(c),'-o',str(exe)],check=True)
    result = subprocess.run([str(exe)])
    print(name, 'exit', result.returncode, 'expected', expected, flush=True)
    assert result.returncode == expected
