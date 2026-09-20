#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Verify that I2C tool wrappers return the negative errno callers report."""

from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r"\nint " + name + r"\([^;]*?\n\{", source)
    assert match
    start = match.start() + 1
    position = match.end()
    depth = 1
    while depth:
        depth += (source[position] == "{") - (source[position] == "}")
        position += 1
    return source[start:position] + "\n"


def main():
    source = Path(__file__).with_name("i2c_devif.c").read_text()
    fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#define FAR
#define I2CIOC_TRANSFER 1
#define I2CIOC_RESET 2
struct i2c_msg_s { int unused; };
struct i2c_transfer_s { struct i2c_msg_s *msgv; int msgc; };
static struct i2c_msg_s messages[2];
static int ioctl_result, ioctl_errno;
static int ioctl(int fd, int request, unsigned long argument)
{
  assert(fd == 12);
  if (request == I2CIOC_TRANSFER) {
    struct i2c_transfer_s *transfer = (void *)(uintptr_t)argument;
    assert(transfer->msgv == messages && transfer->msgc == 2);
  } else { assert(request == I2CIOC_RESET && argument == 0); }
  errno = ioctl_errno;
  return ioctl_result;
}
'''
    for name in ("i2cdev_transfer", "i2cdev_reset"):
        fixture += function(source, name)
    fixture += r'''
int main(void)
{
  ioctl_result = 0;
  ioctl_errno = EINVAL; /* Ignore stale errno after success. */
  assert(i2cdev_transfer(12, messages, 2) == 0 && i2cdev_reset(12) == 0);
  ioctl_result = -1;
  ioctl_errno = ETIMEDOUT;
  assert(i2cdev_transfer(12, messages, 2) == -ETIMEDOUT);
  assert(i2cdev_reset(12) == -ETIMEDOUT);
  ioctl_errno = EIO;
  assert(i2cdev_transfer(12, messages, 2) == -EIO && i2cdev_reset(12) == -EIO);
  puts("I2CTOOL_ERRNO=PASS success/stale errno/timeout/bus error");
}
'''
    with tempfile.TemporaryDirectory(prefix="i2ctool-errno-") as directory:
        path = Path(directory)
        (path / "test.c").write_text(fixture)
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        str(path / "test.c"), "-o", str(path / "test")], check=True)
        subprocess.run([str(path / "test")], check=True)


if __name__ == "__main__":
    main()
