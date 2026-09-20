#!/usr/bin/env python3
from pathlib import Path
import tempfile,subprocess,re
root=Path(__file__).resolve().parents[3]
b=root/'openvela-dev/external/zblue/zblue'
def source(p):
 return re.sub(r'^#include[^\n]*\n','',p.read_text(),flags=re.M)
pre=r'''
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#define CONFIG_SETTINGS_ENCODE_LEN 1
#define CONFIG_SETTINGS_FILE_MAX_LINES 1
#define CONFIG_SETTINGS_FILE_PATH "settings"
#define CONFIG_SETTINGS_LOG_LEVEL 0
#define LOG_MODULE_DECLARE(...)
#define LOG_DBG(...)
#define LOG_ERR(...)
#define MIN(a,b) ((a)<(b)?(a):(b))
#define ROUND_UP(x,a) (((x)+(a)-1)/(a)*(a))
#define CONTAINER_OF(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
#define SETTINGS_MAX_NAME_LEN 64
#define SETTINGS_EXTRA_LEN 8
#define FS_O_READ 1
#define FS_O_WRITE 2
#define FS_O_RDWR 3
#define FS_O_CREATE 0x10
#define FS_O_APPEND 0x20
#define FS_O_TRUNC 0x40
#define FS_SEEK_SET 0
#define FS_SEEK_CUR 1
#define FS_SEEK_END 2
#define FS_DIR_ENTRY_DIR 1
#define FS_DIR_ENTRY_FILE 2
#define SETTINGS_FILE_NAME_MAX 32
typedef int fs_mode_t;
struct fs_file_t { FILE *filep; };
struct fs_dir_t { int unused; };
struct fs_statvfs { int unused; };
struct fs_dirent { char name[64]; int type; size_t size; };
static void fs_file_t_init(struct fs_file_t *f) { memset(f,0,sizeof(*f)); }
struct settings_store;
struct settings_load_arg { const char *subtree; };
struct settings_store_itf { int (*csi_load)(struct settings_store*,const struct settings_load_arg*); int (*csi_save)(struct settings_store*,const char*,const char*,size_t); void *(*csi_storage_get)(struct settings_store*); };
struct settings_store { const struct settings_store_itf *cs_itf; };
struct settings_file { struct settings_store cf_store; const char *cf_name; int cf_maxlines,cf_lines; };
struct settings_store *src,*dst;
static void settings_src_register(struct settings_store *s) { src=s; }
static void settings_dst_register(struct settings_store *s) { dst=s; }
struct settings_line_dup_check_arg { const char *name,*val; size_t val_len; int is_dup; };
struct settings_line_read_value_cb_ctx { void *read_cb_ctx; off_t off; };
struct line_entry_ctx {void *stor_ctx;off_t seek;size_t len;};
typedef int (*line_load_cb)(const char*,void*,off_t,void*);
static char loaded[256];static int calls,handler_error;
static int settings_call_set_handler(const char *name,size_t len,ssize_t (*cb)(void*,void*,size_t),void *arg,const struct settings_load_arg *load) {
 (void)load; assert(len<sizeof(loaded)); assert(cb(arg,loaded,len)==(ssize_t)len); loaded[len]=0;calls++; (void)name;return handler_error;
}
int settings_line_len_calc(const char*,size_t);
int settings_line_raw_read(off_t,char*,size_t,size_t*,void*);
void settings_mount_file_backend(struct settings_file*);
'''
faults=r'''
static int fault, compressing;
static int fault_open(struct fs_file_t *f,const char *name,fs_mode_t flags) {
 int rc=fs_open(f,name,flags);if(!rc && strstr(name,".cmp")) compressing=1;return rc;
}
static ssize_t fault_read(struct fs_file_t *f,void *data,size_t n) {
 if(fault==1 && compressing) return -EIO;return fs_read(f,data,n);
}
static int fault_sync(struct fs_file_t *f) {return fault==2?-ENOSPC:fs_sync(f);}
static int fault_rename(const char *a,const char *b) {return fault==3?-EIO:fs_rename(a,b);}
#define fs_open fault_open
#define fs_read fault_read
#define fs_sync fault_sync
#define fs_rename fault_rename
'''
test=r'''
int main(void) {
 struct fs_file_t f;struct fs_dirent info;
 assert(fs_open(&f,"absent",FS_O_READ)==-ENOENT);
 assert(fs_stat("absent",&info)==-ENOENT);
 assert(fs_open(&f,"flags",FS_O_CREATE|FS_O_RDWR)==0);
 assert(fs_write(&f,"abcd",4)==4);assert(fs_seek(&f,1,FS_SEEK_SET)==0);
 assert(fs_write(&f,"Z",1)==1);assert(fs_sync(&f)==0);assert(fs_close(&f)==0);
 assert(fs_open(&f,"flags",FS_O_READ)==0);char data[8]={0};assert(fs_read(&f,data,4)==4);assert(!memcmp(data,"aZcd",4));fs_close(&f);
 assert(fs_open(&f,"flags",FS_O_RDWR|FS_O_TRUNC)==0);fs_close(&f);assert(fs_stat("flags",&info)==0&&info.size==0);
 assert(settings_backend_init()==0);assert(src->cs_itf->csi_load(src,NULL)==0);
 const char *keys[]={"bt/mesh/Net","bt/mesh/IV","bt/mesh/Seq","bt/mesh/RPL/0001","bt/mesh/AppKey/000"};
 for(int round=0;round<30;round++) for(unsigned i=0;i<5;i++) { char v[20];snprintf(v,sizeof(v),"value-%d-%u",round,i);assert(dst->cs_itf->csi_save(dst,keys[i],v,strlen(v))==0); }
 calls=0;assert(src->cs_itf->csi_load(src,NULL)==0&&calls==5);
 assert(dst->cs_itf->csi_save(dst,keys[4],NULL,0)==0);calls=0;assert(src->cs_itf->csi_load(src,NULL)==0&&calls==4);
 for(int fmode=1;fmode<=3;fmode++) {
  compressing=0;fault=fmode;assert(dst->cs_itf->csi_save(dst,keys[0],"badreplacement",14)<0);
  fault=0;calls=0;assert(src->cs_itf->csi_load(src,NULL)==0&&calls==4);
 }
 handler_error=-EIO;assert(src->cs_itf->csi_load(src,NULL)==-EIO);handler_error=0;
 assert(fs_open(&f,"settings",FS_O_RDWR|FS_O_APPEND)==0);assert(fs_write(&f,"x",1)==1);fs_close(&f);
 assert(src->cs_itf->csi_load(src,NULL)<0);assert(dst->cs_itf->csi_save(dst,keys[0],"new",3)<0);
 puts("PASS actual FILE backend: flags, overwrite, truncate, errno, 150 atomic compactions, Mesh key classes, deletion, handler failure, torn record rejected");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d); text=pre+source(b/'port/subsys/fs/fs.c')+source(b/'subsys/settings/src/settings_line.c')+faults+source(b/'subsys/settings/src/settings_file.c')+test
 (p/'test.c').write_text(text)
 subprocess.run(['cc','-g','-fsanitize=address,undefined','-Werror=implicit-function-declaration',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)
