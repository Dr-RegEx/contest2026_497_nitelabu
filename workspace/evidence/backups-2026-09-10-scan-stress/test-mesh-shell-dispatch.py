from pathlib import Path
import subprocess,tempfile
src=Path('openvela-dev/external/zblue/zblue/port/subsys/shell/shell.c').read_text()
fn=src[src.index('static int execute_cmd('):src.index('extern void z_sys_init')]
h='''#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
struct shell;
struct shell_static_entry;
struct sub { const struct shell_static_entry *entry; };
struct shell_static_entry {const char *syntax; const struct sub *subcmd; int (*handler)(const struct shell*,size_t,char**); struct {int mandatory;} args;};
struct ctx {struct shell_static_entry active_cmd;};
struct shell {struct ctx *ctx;};
static int calls;
static int leaf(const struct shell*s,size_t n,char**a){assert(n==2);assert(!strcmp(a[0],"pb-adv"));assert(!strcmp(a[1],"on"));calls++;return 7;}
static int init(const struct shell*s,size_t n,char**a){assert(n==1);return 8;}
static const struct shell_static_entry leaves[]={{"pb-adv",0,leaf,{2}},{0}};
static const struct sub prov={leaves};
static const struct shell_static_entry groups[]={{"prov",&prov,0,{0}},{"init",0,init,{1}},{0}};
static const struct sub mesh={groups};
static const struct shell_static_entry root={"mesh",&mesh,0,{0}};
static const struct shell_static_entry *root_cmd_find(const char*s){return strcmp(s,"mesh")?0:&root;}
static void shell_help(const struct shell*s){}
'''
t='''int main(void){struct ctx ctx={0};struct shell sh={&ctx};char *good[]={"mesh","prov","pb-adv","on"};char *start[]={"mesh","init"};char *bad[]={"mesh","prov","bad"};char *unknown[]={"bad"};assert(execute_cmd(&sh,4,good)==7);assert(calls==1);assert(execute_cmd(&sh,3,good)==-EINVAL);assert(calls==1);assert(execute_cmd(&sh,2,start)==8);assert(execute_cmd(&sh,3,bad)==-ENOEXEC);assert(execute_cmd(&sh,1,unknown)==-ENOEXEC);assert(execute_cmd(&sh,1,good)==0);puts("PASS: nested Mesh command, argument forwarding, missing argument, unknown command, init and group help");}'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(h+fn+t);subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(Path(d)/'test')],check=True);subprocess.run([str(Path(d)/'test')],check=True)
