#!/usr/bin/env bash
# Apply the PCIe overlays used on the reference Pi 5. Requires reboot.
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
  echo "Appending PCIe overlays to $CFG [all]"
  sudo tee -a "$CFG" >/dev/null <<'EOF'

# HamGeek M2SDR (MyB210)
[all]
dtoverlay=pciex1-compat-pi5,l1ss=off,no-l0s=on,no-mip=off
dtoverlay=pcie-32bit-dma-pi5
dtparam=pciex1
#dtparam=pciex1_gen3
# Uncomment the next line for 4 KiB pages if streaming hits "dma buff MMAP failed":
#kernel=kernel8.img
EOF
else
  echo "PCIe overlays already present in $CFG"
fi

if ! grep -q 'pcie_aspm=off' "$CMD"; then
  echo "Adding pci=noaer pcie_aspm=off to cmdline"
  sudo sed -i 's/$/ pci=noaer pcie_aspm=off/' "$CMD"
else
  echo "cmdline already has pcie_aspm=off"
fi

echo
echo "Backups: ${CFG}.bak.m2sdr-${TS}  ${CMD}.bak.m2sdr-${TS}"
echo "Reboot (power cycle preferred) then check:"
echo "  lspci | grep -i xil"
echo "  getconf PAGESIZE"
