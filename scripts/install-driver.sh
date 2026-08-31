#!/usr/bin/env bash
# Build and DKMS-install mymodule, then load it and create /dev/FPGA.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/pcie-driver"
VER=0.25

if [[ ! -d /lib/modules/$(uname -r)/build ]]; then
  echo "No kernel build dir for $(uname -r)." >&2
  echo "Debian/Ubuntu: sudo apt-get install linux-headers-\$(uname -r) build-essential dkms" >&2
  echo "Fedora:        sudo dnf install kernel-devel dkms gcc make" >&2
  echo "Arch:          sudo pacman -S linux-headers dkms base-devel" >&2
  exit 1
fi

sudo mkdir -p "/usr/src/m2sdr-${VER}"
sudo cp "$SRC/mymodule.c" "$SRC/Makefile" "$SRC/dkms.conf" "/usr/src/m2sdr-${VER}/"

if dkms status -m m2sdr -v "$VER" 2>/dev/null | grep -q installed; then
  sudo dkms remove -m m2sdr -v "$VER" --all || true
fi
sudo dkms add -m m2sdr -v "$VER" || true
sudo dkms build -m m2sdr -v "$VER"
sudo dkms install -m m2sdr -v "$VER" --force

sudo cp "$ROOT/udev/99-m2sdr.rules" /etc/udev/rules.d/
sudo cp "$ROOT/modules-load.d/m2sdr.conf" /etc/modules-load.d/
sudo udevadm control --reload-rules
sudo udevadm trigger || true

sudo modprobe -r mymodule 2>/dev/null || true
sudo modprobe mymodule

if [[ ! -e /dev/FPGA ]]; then
  echo "/dev/FPGA missing after modprobe; falling back to load_module.sh" >&2
  sudo "$SRC/load_module.sh" reload
fi

echo "----"
ls -l /dev/FPGA
lspci -nnk | grep -A4 -E '10ee:70' || true
echo "Driver install finished."
