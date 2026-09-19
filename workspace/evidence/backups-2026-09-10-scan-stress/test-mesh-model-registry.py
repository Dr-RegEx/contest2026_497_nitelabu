"""Exercise actual NuttX Mesh registry with synthetic model handlers on host."""
from pathlib import Path
import subprocess
import tempfile
base=Path('openvela-dev/external/zblue/zblue/subsys/bluetooth/mesh/shell').resolve()
s=(base/'shell.c').read_text()
a=s.index('#ifdef __NuttX__\n/* Keep model handlers')
b=s.index('\nstatic int cmd_init(',a)
registry=s[a:b]
prefix='''#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define __NuttX__ 1
#define CONFIG_BT_MESH_SHELL_CFG_CLI 1
#define CONFIG_BT_MESH_SHELL_HEALTH_CLI 1
#define CONFIG_BT_MESH_SHELL_SAR_CFG_CLI 1
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
struct shell_static_entry { const char *syntax; int (*handler)(void); };
union shell_cmd_entry { const struct shell_static_entry *entry; };
static int cfg(void){return 1;}
static int health(void){return 2;}
static int sar(void){return 3;}
const struct shell_static_entry shell_subcmds_mesh_models_cfg={"cfg",cfg};
const struct shell_static_entry shell_subcmds_mesh_models_health={"health",health};
const struct shell_static_entry shell_subcmds_mesh_models_sar={"sar",sar};
'''
tail='''int main(void){nuttx_models_init();assert(ARRAY_SIZE(nuttx_model_entries)==4);const char *names[]={"cfg","health","sar"};for(int i=0;i<3;i++){assert(!strcmp(model_cmds.entry[i].syntax,names[i]));assert(model_cmds.entry[i].handler()==i+1);}assert(!model_cmds.entry[3].syntax);puts("PASS: cfg/health/SAR retained, registered, callable and null-terminated");}'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(prefix+registry+tail)
 exe=Path(d)/'test'
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I',str(base),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
