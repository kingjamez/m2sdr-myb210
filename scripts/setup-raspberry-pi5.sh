#!/usr/bin/env bash
# Pi 5 firmware for HamGeek M2SDR streaming. Requires reboot / power cycle.
# Does NOT add cma=64M@1024M (that kills HDMI).
set -euo pipefail

if [[ ! -f /boot/firmware/config.txt ]]; then
  echo "Not a Raspberry Pi firmware layout (/boot/firmware/config.txt missing)." >&2
  exit 1
fi

CFG=/boot/firmware/config.txt
CMD=/boot/firmware/cmdline.txt
TS="$(date +%Y%m%d%H%M%S)"

sudo cp "$CFG" "${CFG}.bak.m2sdr-${TS}"
sudo cp "$CMD" "${CMD}.bak.m2sdr-${TS}"

if ! grep -q 'pcie-32bit-dma-pi5' "$CFG"; then
  echo "Appending PCIe overlays and 4 KiB kernel to $CFG [all]"
  sudo tee -a "$CFG" >/dev/null <<'EOF'

# HamGeek M2SDR (MyB210)
[all]
kernel=kernel8.img
dtoverlay=pciex1-compat-pi5,l1ss=off,no-l0s=on,no-mip=off
dtoverlay=pcie-32bit-dma-pi5
dtparam=pciex1
#dtparam=pciex1_gen3
EOF
else
  echo "PCIe overlays already present in $CFG"
  if ! grep -q '^kernel=kernel8.img' "$CFG"; then
    echo "Adding kernel=kernel8.img for 4 KiB pages"
    sudo tee -a "$CFG" >/dev/null <<'EOF'

# 4 KiB pages so HamGeek libpcie mmap() works (M2SDR streaming)
kernel=kernel8.img
EOF
  fi
fi

# cmdline extras. Never cma=64M@1024M.
if grep -q 'cma=64M@1024M' "$CMD"; then
  echo "Removing cma=64M@1024M (it breaks HDMI)"
  sudo sed -i 's/ *cma=64M@1024M//g' "$CMD"
fi
for tok in pci=noaer pcie_aspm=off numa=fake=1 iommu_dma_numa_policy=default; do
  if ! grep -q "$tok" "$CMD"; then
    echo "Adding $tok to cmdline"
    sudo sed -i "s/\$/ $tok/" "$CMD"
  fi
done

echo
echo "Backups: ${CFG}.bak.m2sdr-${TS}  ${CMD}.bak.m2sdr-${TS}"
echo "Power-cycle, then:"
echo "  getconf PAGESIZE          # 4096"
echo "  uname -r                  # ...-rpi-v8"
echo "  ./scripts/install-driver.sh"
echo "  ./scripts/verify-rx.sh"
