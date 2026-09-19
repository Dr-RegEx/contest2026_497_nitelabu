#!/usr/bin/env python3
"""Run the production cache lookup against two independent controller contexts."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
src = (root / 'openvela-dev/external/zblue/zblue/subsys/bluetooth/host/gatt.c').read_text()
start = src.index('static struct gatt_cf_cfg *find_cf_cfg(')
end = src.index('\n}', start) + 2
helper = src[start:end]
preamble = '''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
typedef struct { uint8_t val; } bt_addr_le_t;
static const bt_addr_le_t any = { 0 };
#define BT_ADDR_LE_ANY (&any)
struct gatt_cf_cfg { uint8_t id; bt_addr_le_t peer; };
struct bt_dev_gatt_ctx { struct gatt_cf_cfg cf_cfg[2]; };
struct bt_dev { struct bt_dev_gatt_ctx *gatt_ctx; };
struct bt_conn { struct bt_dev *hdev; uint8_t id; bt_addr_le_t peer; };
static int bt_addr_le_eq(const bt_addr_le_t *a, const bt_addr_le_t *b) { return a->val == b->val; }
static int bt_conn_is_peer_addr_le(struct bt_conn *c, uint8_t id, const bt_addr_le_t *p) { return c->id == id && c->peer.val == p->val; }
'''
test = '''
int main(void) {
 struct bt_dev_gatt_ctx a = { .cf_cfg = {{1,{42}},{0,{0}}} };
 struct bt_dev_gatt_ctx b = { .cf_cfg = {{1,{42}},{2,{24}}} };
 struct bt_dev da = {&a}, db = {&b};
 struct bt_conn ca = {&da,1,{42}}, cb = {&db,1,{42}};
 assert(find_cf_cfg(ca.hdev,&ca) == &a.cf_cfg[0]);
 assert(find_cf_cfg(cb.hdev,&cb) == &b.cf_cfg[0]);
 assert(find_cf_cfg(&da,NULL) == &a.cf_cfg[1]);
 assert(find_cf_cfg(&db,NULL) == NULL);
 ca.id=2; assert(find_cf_cfg(&da,&ca) == NULL);
 cb.id=2; cb.peer.val=24; assert(find_cf_cfg(&db,&cb) == &b.cf_cfg[1]);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='s31-gatt-cache-') as tmp:
 p=Path(tmp)/'test.c';p.write_text(preamble+helper+test)
 exe=Path(tmp)/'test'
 subprocess.run(['cc','-std=c11','-Wall','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('production find_cf_cfg: two-controller same-peer, identity and free-slot isolation PASS')
