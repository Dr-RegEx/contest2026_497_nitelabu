#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'openvela-dev/frameworks/connectivity/bluetooth/service/stacks/zephyr/hci_h4.c').read_text()
def fn(name):
 start=s.index('static int '+name+'('); b=s.index('{',start); end=b+1; depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}'); end+=1
 return s[start:end]
pre=r'''
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#define BT_LOGE(...) ((void)0)
#define BT_HCI_H4_ACL 2
#define BT_HCI_H4_CMD 1
#define BT_HCI_H4_ISO 5
#define BT_BUF_ACL_OUT 2
#define BT_BUF_CMD 1
#define BT_BUF_ISO_OUT 5
#define CONFIG_BT_ISO 1
#define IS_ENABLED(x) (x)
struct net_buf { uint8_t store[32]; uint8_t *data; unsigned len; int type, refs; };
struct h4_data { int fd; pthread_mutex_t mutex; };
struct device { struct h4_data *data; };
static int bt_buf_get_type(struct net_buf *b) { return b->type; }
static void net_buf_push_u8(struct net_buf *b,uint8_t v) { *--b->data=v;b->len++; }
static uint8_t net_buf_pull_u8(struct net_buf *b) { b->len--;return *b->data++; }
static void net_buf_unref(struct net_buf *b) { assert(b->refs>0);b->refs--; }
#define h4_data_dump(...) ((void)0)
static int failmode, writes, last_fd;
static ssize_t test_write(int fd,const void *p,size_t n) {
 (void)p; last_fd=fd;writes++;
 if(failmode==1) {errno=ENOSPC; return -1;}
 if(failmode==2) return 0;
 if(failmode==3 && writes==1) {errno=EINTR;return -1;}
 return n;
}
#define write test_write
'''
test=r'''
int main(void) {
 struct h4_data a={.fd=7,.mutex=PTHREAD_MUTEX_INITIALIZER}, b={.fd=9,.mutex=PTHREAD_MUTEX_INITIALIZER};
 struct device devs[2]={{&a},{&b}};
 for(int d=0;d<2;d++) for(int m=0;m<5;m++) {
  struct net_buf x={.len=4,.type=BT_BUF_ISO_OUT,.refs=1}; x.data=x.store+4;
  failmode=m; writes=0; devs[d].data->fd=m==4?-1:(d?9:7);
  int ret=h4_send(&devs[d],&x);
  if(m==0||m==3) {assert(ret==0&&x.refs==0&&last_fd==(d?9:7));}
  else {assert(ret==(m==1?-ENOSPC:m==2?-EIO:-ENODEV));assert(x.refs==1&&x.len==4&&x.data==x.store+4);net_buf_unref(&x);}
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(pre+fn('h4_send_data')+fn('h4_send')+test)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-pthread',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS production H4 send: ISO success, two devices, ENOSPC, zero write, EINTR, closed fd, caller-owned error buffer/header rollback')
