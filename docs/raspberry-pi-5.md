# Raspberry Pi 5 notes

Reference hardware: Pi 5 Model B Rev 1.1, Debian 13, 52Pi EP-0180 (ASM1184e 4-port PCIe Gen2 x1 switch), HamGeek M2SDR on switch downstream port (Xilinx `10ee:7022`).

The Pi 5 FFC is PCIe **x1**. The M2SDR bitstream advertises x2; the link trains **5 GT/s x1 (downgraded)**. That is expected.

## Boot firmware

`/boot/firmware/config.txt`:

```ini
[all]
dtoverlay=pciex1-compat-pi5,l1ss=off,no-l0s=on,no-mip=off
dtoverlay=pcie-32bit-dma-pi5
dtparam=pciex1
#dtparam=pciex1_gen3
```

- `pcie-32bit-dma-pi5` is required: the FPGA BARs/DMA mask are 32-bit.
- `pciex1-compat-pi5` disables L1SS/L0s (the card misbehaves with ASPM).
- Do **not** enable Gen3 unless you are chasing NVMe speed on the same switch; the switch is Gen2.

`/boot/firmware/cmdline.txt` extra tokens:

```text
pci=noaer pcie_aspm=off
```

EEPROM should have `PCIE_PROBE=1` (Pi 5 default in current firmware).

Apply with a full power cycle, not only `reboot`, after changing overlays.

## 16 KiB pages vs 4 KiB `mmap`

Default Pi 5 kernel `kernel_2712.img` / `*-rpi-2712` uses **16 KiB** pages (`getconf PAGESIZE` → 16384).

Vendor `arm_libpcie.a` issues `mmap` with length/offset **4096**. On Linux the offset must be a multiple of `PAGE_SIZE`, so DMA buffer mapping fails:

```text
dma buff MMAP failed
Timeout while streaming
```

Discovery still works because BAR0 is mapped with `offset=0`.

### Workaround (not verified after reboot on the reference host)

1. Confirm `kernel8.img` exists (`/boot/firmware/kernel8.img`) and install `linux-headers-*-rpi-v8`.
2. Add to `[all]` in `config.txt`:

   ```ini
   kernel=kernel8.img
   ```

3. Reboot. `uname -r` should end in `rpi-v8`, `getconf PAGESIZE` should be `4096`.
4. Rebuild DKMS against the new kernel:

   ```bash
   sudo dkms install -m m2sdr -v 0.25 --force
   sudo modprobe mymodule
   ```

Keep `pcie-32bit-dma-pi5`; 4 KiB pages do not remove the 32-bit DMA need.

## Coexistence with NVMe on EP-0180

The switch uplink is the Pi 5 x1 link. NVMe and the SDR can share the switch; they share that uplink. Empty switch ports stay at 2.5 GT/s x1 with no child device. After moving an SSD, power-cycle; `echo 1 > /sys/bus/pci/rescan` is not enough.

## Do not use XDMA

`10ee:7022` matches Xilinx XDMA’s default ID. Official `xdma.ko` probe fails (`Failed to detect XDMA config BAR`). This FPGA is a 4 KiB AXI-Lite endpoint, not XDMA.
