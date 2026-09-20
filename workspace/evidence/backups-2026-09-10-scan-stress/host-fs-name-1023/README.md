# NAME_MAX intermediate directory boundary regression

The actual original `_inode_checkpath` rejects a32-byte component followed by
slash, even though a terminal32-byte name is accepted. mkdir(relative32) and
stat(relative32) bypass that prior component because validation precedes cwd
expansion; chdir's realpath then calls lstat with the expanded absolute path,
which fails at the earlier32-byte component. The1533 failure path is83bytes.

The fix removes the name-length condition from the traversal guard and rejects
only a33rd non-slash byte. PATH_MAX loop and final result remain unchanged.
No original xTS source, workload, NAME_MAX or PATH_MAX configuration changed.

`actual.c` contains the source function after correction and original HEAD
function (renamed only). `check.c` verifies32 terminal,32/slash/component,33,
empty path, original failed83-byte path, and old/new256/257-byte path semantics.
ASan/UBSan PASS; not hardware evidence.

```
cc -std=c11 -O1 -g -fsanitize=address,undefined backups/2026-09-10-scan-stress/host-fs-name-1023/check.c -o /tmp/fs-name-1023
/tmp/fs-name-1023
```
