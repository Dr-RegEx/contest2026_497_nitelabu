#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NUTTX="$ROOT/openvela-dev/nuttx"
STM32="$NUTTX/arch/arm/src/stm32f7/stm32_otghost.c"
S31="$NUTTX/arch/risc-v/src/esp32s31/esp32s31_usbhost.c"
OTG="$NUTTX/arch/risc-v/src/esp32s31/hardware/esp32s31_otg.h"
DWCS="$NUTTX/arch/risc-v/src/esp32s31/esp-hal-3rdparty/components/soc/esp32s31/include/soc/usb_dwc_struct.h"

echo USB1780_DWC2_REUSE_CHECK_BEGIN
test -s "$STM32" && echo STM32_DWC2_HOST_SOURCE=present
test -s "$S31" && echo S31_HOST_CANDIDATE_SOURCE=present

callbacks=(wait enumerate ep0configure epalloc epfree alloc free ioalloc iofree ctrlin ctrlout transfer cancel disconnect)
count=0
for callback in "${callbacks[@]}"; do
  if grep -q "stm32_${callback}" "$STM32"; then
    count=$((count + 1))
  fi
done
echo STM32_NUTTX_CALLBACKS="$count/${#callbacks[@]}"

for reg in HCFG HFNUM HAINT HAINTMSK HPRT HCCHAR HCINT HCINTMSK HCTSIZ HCDMA HCDMAB; do
  grep -q "ESP32S31_OTG_${reg}" "$OTG" || exit 1
done
echo S31_HOST_REG_SYMBOLS=present
grep -q 'host_chans\[16\]' "$DWCS" && echo S31_HOST_CHANNEL_COUNT=16

python3 - "$STM32" "$S31" <<'PY'
import re
import sys

stm32 = open(sys.argv[1], encoding="utf-8").read()
s31 = open(sys.argv[2], encoding="utf-8").read()
required = ("stm32_chan_configure", "stm32_transfer_start", "stm32_gint_hcisr",
            "stm32_gint_hprtisr", "stm32_wait", "stm32_enumerate")
missing = [name for name in required if name not in stm32]
print("STM32_DWC2_ENGINE=" + ("present" if not missing else "missing:" + ",".join(missing)))
candidate = [name for name in ("wait", "enumerate", "epalloc", "transfer", "cancel")
             if re.search(r"\b" + re.escape(name) + r"\b", s31)]
print("S31_HOST_HCD_IMPLEMENTATION=" + ("init-only" if not candidate else "partial:" + ",".join(candidate)))
PY

echo REUSE_CONCLUSION=PORT_STM32_DWC2_HCD_WITH_S31_PLATFORM_ADAPTER
echo S31_PLATFORM_GAPS=IRQ_VBUS_CLOCK_PHY_FIFO_CONFIG_DMA_CACHE_BOARD_BRINGUP
echo USB1780_STATUS=HCD_NOT_IMPLEMENTED
echo USB1780_DWC2_REUSE_CHECK_END
