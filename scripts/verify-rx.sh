#!/usr/bin/env bash
# Prove the card streams. Never rmmod mymodule after this.
set -uo pipefail
export PATH="/usr/local/bin:/opt/m2sdr-uhd/bin:/usr/bin:/bin${PATH:+:$PATH}"

echo "===== $(date -Is) M2SDR verify-rx ====="
uname -a
echo "PAGESIZE=$(getconf PAGESIZE)"
if [[ "$(getconf PAGESIZE)" != "4096" ]]; then
  echo "ERROR: PAGE_SIZE is not 4096. Vendor libpcie mmap() will fail." >&2
  echo "Pi 5: boot kernel8.img (see docs/raspberry-pi-5.md)." >&2
  exit 1
fi
findmnt / || true
echo "cmdline: $(tr -d '\n' < /proc/cmdline)"
if grep -q 'cma=64M@1024M' /proc/cmdline; then
  echo "WARNING: cma=64M@1024M is set. HDMI/GPU will likely die. Remove it." >&2
fi
echo

echo "=== PCIe ==="
lspci -nnk | grep -A5 -E '10ee:70' || true
echo
modinfo mymodule 2>/dev/null | grep -E 'filename|srcversion|vermagic' || true
lsmod | grep mymodule || true
if ! lsmod | grep -q mymodule; then
  echo "loading mymodule..."
  sudo modprobe mymodule || true
fi
ls -l /dev/FPGA || { echo "ERROR: /dev/FPGA missing"; exit 1; }
echo

uhd_find_devices || sudo uhd_find_devices || true

EX=""
for c in \
  /usr/local/lib/uhd/examples/rx_samples_to_file \
  /opt/m2sdr-uhd/lib/uhd/examples/rx_samples_to_file \
  "$(command -v rx_samples_to_file 2>/dev/null || true)"
do
  if [[ -n "$c" && -x "$c" ]]; then EX=$c; break; fi
done
if [[ -z "$EX" ]]; then
  echo "ERROR: rx_samples_to_file not found. Build vendor UHD (scripts/install-uhd.sh)." >&2
  exit 1
fi

rm -f /tmp/m2sdr_rx.dat
set +e
sudo "$EX" --args "type=b200,recv_frame_size=8176,num_recv_frames=64" \
  --nsamps 20000 --rate 1e6 --freq 100e6 --gain 40 \
  --file /tmp/m2sdr_rx.dat --duration 0.05
echo "rx_exit=$?"
set -e
ls -l /tmp/m2sdr_rx.dat 2>/dev/null || true
grep -H . /proc/interrupts | grep -i m2sdr || true
if command -v dmesg >/dev/null; then
  dmesg | grep -iE 'm2sdr: (1GiB pool|alloc idx=0 |alloc idx=1 |noncoherent reject|mmap_23|dma buff)' | tail -n 40 || true
fi
echo "===== done $(date -Is) ====="

if [[ -s /tmp/m2sdr_rx.dat ]]; then
  echo "OK: captured $(wc -c < /tmp/m2sdr_rx.dat) bytes"
  exit 0
fi
echo "FAIL: no samples. See docs/raspberry-pi-5.md and dmesg." >&2
exit 1
