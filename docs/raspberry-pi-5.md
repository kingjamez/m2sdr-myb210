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
| Driver | community `mymodule.c` |
| Verified | `rx_samples_to_file --args type=b200 --nsamps 20000 --rate 1e6` → `rx_exit=0`, HDMI stayed up |
| Sustained RX ceiling | **20 MS/s** in SDR++ (`recv_frame_size=8176,num_recv_frames=64`). 16 MS/s is conservative. 24+ dies in seconds. [sample-rate.md](sample-rate.md) |

The Pi 5 FFC is PCIe **x1**. The M2SDR bitstream advertises x2; the link trains **5 GT/s x1**. Booting the OS from SD does **not** widen the FPGA link.

## 1. Firmware overlays (required)

`/boot/firmware/config.txt` `[all]`:

```ini
[all]
kernel=kernel8.img
dtoverlay=pciex1-compat-pi5,l1ss=off,no-l0s=on,no-mip=off
dtoverlay=pcie-32bit-dma-pi5
dtparam=pciex1
```

- `pcie-32bit-dma-pi5` is required: FPGA BARs/DMA are 32-bit.
- `pciex1-compat-pi5` disables L1SS/L0s.
- Do **not** enable Gen3; the switch is Gen2.

Or run `scripts/setup-raspberry-pi5.sh` and reboot.

## 2. cmdline

```text
pci=noaer pcie_aspm=off numa=fake=1 iommu_dma_numa_policy=default
```

**Never** add `cma=64M@1024M`. That can make RX work and **kills HDMI**. The community driver carves FPGA DMA at **1 GiB** (`0x40000000`) and leaves default CMA for `vc4`.

## 3. Confirm the 4 KiB kernel

```bash
getconf PAGESIZE          # 4096
uname -r                  # must end in rpi-v8, not rpi-2712
```

## 4. Why the community driver exists

Vendor `dma_alloc_coherent` lands in the Pi 5 CMA hole (`0x3b800000–0x3f7fffff`). This repo’s module uses `DMA_BIT_MASK(31)` and a pool at `0x40000000`.

## 5. Do not

- `rmmod mymodule` after RX (Oops + FPGA D3cold). Reboot.
- Put `cma=64M@1024M` on cmdline.
- Use official `xdma.ko`.
- Expect USB3 B210 rates. **SDR++ on this host is 16–20 MS/s.**

Args: `type=b200,recv_frame_size=8176,num_recv_frames=64`. See [sample-rate.md](sample-rate.md).
