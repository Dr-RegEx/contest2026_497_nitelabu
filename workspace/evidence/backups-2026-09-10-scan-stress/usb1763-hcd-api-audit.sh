#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Offline-only audit: compare the NuttX USB host ABI with the S31 candidate.
# This script deliberately does not build, flash, open a serial port, or
# claim that a missing callback is implemented.
set -eu

ROOT="${1:-$(cd "$(dirname "$0")/../.." && pwd)}"
HEADER="$ROOT/openvela-dev/nuttx/include/nuttx/usb/usbhost.h"
SOURCE="$ROOT/openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_usbhost.c"

test -r "$HEADER"
test -r "$SOURCE"

required=(wait enumerate ep0configure epalloc epfree alloc free ioalloc iofree
          ctrlin ctrlout transfer cancel disconnect)
optional=(asynch connect)
missing=0
printf 'USB1763_HEADER=%s\n' "$HEADER"
printf 'USB1763_SOURCE=%s\n' "$SOURCE"
printf 'USB1763_API_CHECK_BEGIN\n'
for method in "${required[@]}"; do
  if grep -Eq "\(\*${method}\)" "$HEADER"; then
    if grep -Eq "\b${method}\b" "$SOURCE" &&
       grep -Eq "\(\*${method}\)" "$SOURCE"; then
      state=PRESENT
    else
      state=MISSING
      missing=$((missing + 1))
    fi
  else
    state=HEADER_NOT_FOUND
    missing=$((missing + 1))
  fi
  printf '%s=%s\n' "$method" "$state"
done
for method in "${optional[@]}"; do
  if grep -Eq "\(\*${method}\)" "$HEADER"; then
    if grep -Eq "\b${method}\b" "$SOURCE" &&
       grep -Eq "\(\*${method}\)" "$SOURCE"; then
      state=PRESENT
    else
      state=MISSING_CONDITIONAL
    fi
  else
    state=HEADER_NOT_FOUND
  fi
  printf '%s=%s\n' "$method" "$state"
done

for token in HCCHAR HCTSIZ HCDMA HAINT HAINTMSK; do
  if grep -Eq "\b${token}\b" "$SOURCE"; then
    printf 'DWC_%s=PRESENT\n' "$token"
  else
    printf 'DWC_%s=MISSING\n' "$token"
  fi
done
if grep -Eq "usbhost_(waiter_initialize|registerclass|findclass)" "$SOURCE"; then
  printf 'NUTTX_HOST_REGISTRATION=PARTIAL\n'
else
  printf 'NUTTX_HOST_REGISTRATION=MISSING\n'
fi
printf 'USB1763_REQUIRED_MISSING=%s\n' "$missing"
if test "$missing" -eq 0; then
  printf 'USB1763_STATUS=ABI_SURFACE_PRESENT_REVIEW_REQUIRED\n'
else
  printf 'USB1763_STATUS=HCD_INCOMPLETE\n'
fi
printf 'USB1763_API_CHECK_END\n'
