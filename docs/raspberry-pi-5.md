# Raspberry Pi 5

Reference hardware that **streamed IQ samples**:

| Item | Value |
|---|---|
| Host | Raspberry Pi 5 Model B Rev 1.1, Debian 13 (trixie) aarch64 |
| Kernel | `kernel8.img` → `6.12.47+rpt-rpi-v8`, **PAGESIZE=4096** |
| Carrier | 52Pi EP-0180 (ASM1184e PCIe Gen2 x1 switch) |
| Card | `0001:05:00.0` Xilinx `10ee:7022` (subsystem `10ee:0007`) |
| Root disk | NVMe on the same PCIe switch |
| UHD | vendor 4.8.0.0 in `/usr/local` |
| Driver | community `mymodule.c` (`srcversion 65A8816AECE37617225E4AE`) |
| Verified | `rx_samples_to_file --args type=b200 --nsamps 20000 --rate 1e6` → `rx_exit=0`, 80000-byte file, HDMI stayed up |
| Lossless RX ceiling | **44 MS/s** with `recv_frame_size=8176,num_recv_frames=64` (40 MS/s conservative). 50+ drops. [sample-rate.md](sample-rate.md) |

The Pi 5 FFC is PCIe **x1**. The M2SDR bitstream advertises x2; the link trains **5 GT/s x1**. That is expected. Booting the OS from SD (leaving the NVMe idle) does **not** widen the FPGA link.

---

## 1. Firmware overlays (required)

`/boot/firmware/config.txt` `[all]`:

```ini
[all]
# 4 KiB pages so vendor libpcie mmap() works
kernel=kernel8.img
dtoverlay=pciex1-compat-pi5,l1ss=off,no-l0s=on,no-mip=off
dtoverlay=pcie-32bit-dma-pi5
dtparam=pciex1
#dtparam=pciex1_gen3
```

- `pcie-32bit-dma-pi5` is required: FPGA BARs/DMA are 32-bit.
- `pciex1-compat-pi5` disables L1SS/L0s (the card misbehaves with ASPM).
- Do **not** enable Gen3 unless you are chasing NVMe speed on the same switch; the switch is Gen2.
- `auto_initramfs=1` plus `/boot/firmware/initramfs8` should load with `kernel8.img`.

Or run `scripts/setup-raspberry-pi5.sh` and reboot.

## 2. cmdline

Keep (order: last `numa=` wins):

```text
pci=noaer pcie_aspm=off numa=fake=1 iommu_dma_numa_policy=default
```

**Never** add `cma=64M@1024M`. Relocating all CMA to `0x40000000` can make streaming work, but it starves `vc4-kms-v3d` and HDMI sticks on the last firmware line (often ~3.8 s). LightDM logs `Stopping Plymouth, no displays replace it`. The machine may still be running headless.

Default CMA (64 MiB at `0x3b800000`) must stay there so HDMI lives. The community driver carves FPGA DMA from System RAM at **1 GiB** (`0x40000000`) instead.

EEPROM: `PCIE_PROBE=1` (Pi 5 default). After overlay changes, a **power cycle** is safer than `reboot`.

## 3. Confirm the 4 KiB kernel

```bash
getconf PAGESIZE          # 4096
uname -r                  # must end in rpi-v8, not rpi-2712
```

If PAGESIZE is still `16384`, firmware did not take `kernel=kernel8.img`. Rebuild DKMS after the kernel actually switches:

```bash
sudo apt-get install -y linux-headers-$(uname -r)
./scripts/install-driver.sh
```

## 4. Why the community driver exists

Vendor `dma_alloc_coherent` on this Pi lands in `/proc/iomem` `3b800000-3f7fffff : reserved` (the 64 MiB CMA hole at the top of the first GB). The FPGA DMAs there; UHD mmaps other pages; IRQs can fire; userspace sees zeros / `wait_for_ack`.

Relocating CMA with `cma=64M@1024M` moved buffers to `0x40418000` and RX worked **once**, then HDMI died.

The community module instead:

- sets `DMA_BIT_MASK(31)` so addresses stay in the Pi 5 **2 GiB** PCIe inbound window
- `alloc_contig_range` a 16 MiB (fallback 8 MiB) pool in `0x40000000–0x78000000`
- slices 32 KiB DMA buffers from that pool (`dmesg`: `1GiB pool … dma=0x0000000040000000` tagged `pool`)
- mmap PFNs match `dma_addr`

Leave default CMA alone. HDMI stays up. Streaming works.

## 5. Coexistence with NVMe on EP-0180

The switch uplink is the Pi 5 x1 link. NVMe and the SDR can share the switch; they share that uplink. Do not reset the PCIe switch / root port while NVMe is the OS disk. Empty switch ports stay at 2.5 GT/s x1 with no child. After moving an SSD, **power-cycle**.

## 6. Do not

- `rmmod` / `modprobe -r` `mymodule` after any RX attempt (Oops + FPGA D3cold). Reboot to pick up a new module.
- Put `cma=64M@1024M` on cmdline.
- Use official `xdma.ko` (`Failed to detect XDMA config BAR`).
- Expect USB3 B210 rates (61.44 MS/s). The AD9361 will clock 61.44 MHz; this host drops samples above **44 MS/s**. Use `recv_frame_size=8176,num_recv_frames=64`. See [sample-rate.md](sample-rate.md).

## 7. Raising the sample rate

Default HamGeek USB frames (3088 bytes) fail at 16 MS/s. Ettus-sized **8176-byte** frames with **64** buffers are lossless at 16, 32, 40, and 44 MS/s on this NVMe boot. 48 / 50 / 61.44 MS/s clock the radio and then overrun.

```text
type=b200,recv_frame_size=8176,num_recv_frames=64
```

Full table, what we tried that did not help (SD boot, 16360-byte frames, 128 buffers), and the SDR++ patch: **[sample-rate.md](sample-rate.md)**. Quick check: `RATE=40e6 ./scripts/benchmark-rate.sh`.

## Revert to 16 KiB pages

Comment out `kernel=kernel8.img`, reboot, confirm `PAGESIZE=16384` and `uname -r` ends in `rpi-2712`. Streaming will fail again. That is expected.
