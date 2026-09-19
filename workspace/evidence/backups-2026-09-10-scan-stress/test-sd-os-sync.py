"""Compile the actual SD OS adapter against pthread/sem shims, not hardware."""
from pathlib import Path
import subprocess,tempfile
root=Path.cwd(); chip=root/'openvela-dev/nuttx/arch/risc-v/src/esp32s31'
shim=r'''
#ifndef TEST_SHIM_H
#define TEST_SHIM_H
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <sched.h>
#include <limits.h>
#define SEM_PRIO_NONE 0
#define USEC_PER_TICK 1000
#define clock_ticks2time(t,n) do {(t)->tv_sec=(n)/1000; (t)->tv_nsec=((n)%1000)*1000000L;} while(0)
static _Thread_local bool irq_context;
static bool up_interrupt_context(void){return irq_context;}
typedef pthread_mutex_t mutex_t;
typedef pthread_mutex_t spinlock_t;
typedef int irqstate_t;
static int nxmutex_init(mutex_t*m){return -pthread_mutex_init(m,0);}
static int nxmutex_lock(mutex_t*m){return -pthread_mutex_lock(m);}
static int nxmutex_unlock(mutex_t*m){return -pthread_mutex_unlock(m);}
static int nxmutex_destroy(mutex_t*m){return -pthread_mutex_destroy(m);}
static struct timespec deadline(unsigned t){struct timespec d;clock_gettime(CLOCK_REALTIME,&d);d.tv_nsec+=t%1000*1000000L;d.tv_sec+=t/1000+d.tv_nsec/1000000000L;d.tv_nsec%=1000000000L;return d;}
static int nxmutex_ticklock(mutex_t*m,unsigned t){if(!t)return -pthread_mutex_trylock(m);struct timespec d=deadline(t);return -pthread_mutex_timedlock(m,&d);}
static void spin_lock_init(spinlock_t*l){pthread_mutex_init(l,0);}
static irqstate_t spin_lock_irqsave(spinlock_t*l){pthread_mutex_lock(l);return 0;}
static void spin_unlock_irqrestore(spinlock_t*l,irqstate_t f){(void)f;pthread_mutex_unlock(l);}
static int nxsem_init(sem_t*s,int p,unsigned v){return sem_init(s,p,v)?-errno:0;}
static int nxsem_set_protocol(sem_t*s,int p){(void)s;(void)p;return 0;}
static int nxsem_trywait(sem_t*s){return sem_trywait(s)?-errno:0;}
static int nxsem_wait_uninterruptible(sem_t*s){int r;do{r=sem_wait(s);}while(r&&errno==EINTR);return r?-errno:0;}
static int nxsem_tickwait_uninterruptible(sem_t*s,unsigned t){struct timespec d=deadline(t);int r;do{r=sem_timedwait(s,&d);}while(r&&errno==EINTR);return r?-errno:0;}
static int nxsem_get_value(sem_t*s,int*v){return sem_getvalue(s,v)?-errno:0;}
static int nxsem_post(sem_t*s){return sem_post(s)?-errno:0;}
static int nxsem_destroy(sem_t*s){return sem_destroy(s)?-errno:0;}
static int nxsig_nanosleep(struct timespec*r,struct timespec*l){return nanosleep(r,l)?-errno:0;}
static void *heap_caps_calloc(size_t n,size_t z,unsigned c){return c==999?NULL:calloc(n,z);}
static void heap_caps_free(void*p){free(p);}
#endif
'''
test=r'''
#include <assert.h>
#include <stdio.h>
#include "esp32s31_sdmmc_os.c"
static QueueHandle_t stress;
static void *producer(void*p){(void)p;irq_context=true;for(unsigned i=0;i<10000;i++){while(!xQueueSendFromISR(stress,&i,NULL))sched_yield();}return NULL;}
static long long ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000LL+t.tv_nsec/1000000;}
int main(void){
 assert(!xQueueCreateWithCaps(0,4,0));assert(!xQueueCreateWithCaps(2,4,999));
 QueueHandle_t q=xQueueCreateWithCaps(2,sizeof(unsigned),0);assert(q);unsigned a=7,b=9,out=999;BaseType_t wake=0;
 assert(!xQueueReceive(q,&out,0)&&out==999);long long start=ms();assert(!xQueueReceive(q,&out,20));assert(ms()-start>=15);
 irq_context=true;assert(xQueueSendFromISR(q,&a,&wake)&&wake);assert(xQueueSendFromISR(q,&b,NULL));assert(!xQueueSendFromISR(q,&a,NULL));irq_context=false;
 a=88;assert(xQueueReceive(q,&out,0)&&out==7);assert(xQueueSendFromISR(q,&a,NULL));assert(xQueueReceive(q,&out,0)&&out==9);assert(xQueueReceive(q,&out,0)&&out==88);vQueueDeleteWithCaps(q);
 SemaphoreHandle_t s=xSemaphoreCreateBinaryWithCaps(0);assert(s);assert(!xSemaphoreTake(s,0));assert(xSemaphoreGiveFromISR(s,&wake));assert(!xSemaphoreGive(s));assert(xSemaphoreTake(s,0));assert(!xSemaphoreTake(s,10));vSemaphoreDeleteWithCaps(s);
 s=xSemaphoreCreateMutexWithCaps(0);assert(s);assert(xSemaphoreTake(s,0));assert(!xSemaphoreGiveFromISR(s,&wake));assert(xSemaphoreGive(s));vSemaphoreDeleteWithCaps(s);
 stress=xQueueCreateWithCaps(7,sizeof(unsigned),0);pthread_t t;assert(!pthread_create(&t,NULL,producer,NULL));for(unsigned i=0;i<10000;i++){assert(xQueueReceive(stress,&out,1000));assert(out==i);}pthread_join(t,NULL);vQueueDeleteWithCaps(stress);
 start=ms();vTaskDelay(10);assert(ms()-start>=8);vTaskDelay(0);
 puts("PASS: invalid/allocation failure, empty/full, copy/FIFO/wrap, finite timeout, binary cap, mutex ISR rejection, 10000 concurrent events, delay");
}
'''
with tempfile.TemporaryDirectory() as td:
 d=Path(td);(d/'shim.h').write_text(shim)
 for h in ['nuttx/config.h','nuttx/mutex.h','nuttx/semaphore.h','nuttx/signal.h','nuttx/spinlock.h','nuttx/clock.h','esp_heap_caps.h']:
  p=d/h;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#include "shim.h"\n')
 (d/'freertos').mkdir()
 for h in ['semphr.h','queue.h','task.h']:(d/'freertos'/h).write_text((chip/'include/freertos'/h).read_text())
 (d/'freertos/FreeRTOS.h').write_text('#pragma once\n#include <stdint.h>\ntypedef int BaseType_t;typedef unsigned UBaseType_t;typedef uint32_t TickType_t;\n#define pdTRUE 1\n#define pdFALSE 0\n#define portMAX_DELAY UINT32_MAX\n')
 (d/'test.c').write_text(test)
 subprocess.run(['cc','-D_POSIX_C_SOURCE=200809L','-Wall','-Werror','-pthread','-fsanitize=address,undefined','-I',str(d),'-I',str(chip),str(d/'test.c'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
