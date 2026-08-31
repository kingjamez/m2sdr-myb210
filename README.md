# HamGeek M2SDR (UHD name: MyB210)

Community install notes for the HamGeek **M2SDR**: an M.2 2280 PCIe SDR (Xilinx XC7A200 + AD9361) that shows up in UHD as a B210 clone named **MyB210**.

This is **not** Enjoy Digital LiteX-M2SDR (`10ee:7024` / `m2sdr.ko` / SoapySDR). HamGeek’s card is a vendor-patched UHD B210 over a custom PCIe endpoint.

These instructions were reconstructed from a working install on:

| Item | Value |
|---|---|
| Host | Raspberry Pi 5 Model B Rev 1.1 |
| OS | Debian 13 (trixie) aarch64 |
| Kernel | `6.12.47+rpt-rpi-2712` (16 KiB pages) |
| Carrier | 52Pi EP-0180 (ASM1184e PCIe Gen2 x1 switch) |
| Card | `0001:05:00.0` Xilinx `10ee:7022` (subsystem `10ee:0007`) |
| UHD | vendor-patched **4.8.0.0** installed to `/usr/local` |
| What worked | `uhd_find_devices` and `uhd_usrp_probe` see **MyB210** |
| What did not | sample streaming on this 16 KiB-page Pi 5 kernel |

Vendor after-sales package last updated **2026-05-27**. Driver package is `b210_model_pcie_drv_r25`.

---

## What you get from the vendor (and from this repo)

Two pieces are required. Stock Ettus UHD and distro `uhd` packages **will not** see this card.

1. **Linux kernel module** `mymodule` (`FPGA_PCIE`) that binds to PCI IDs `10ee:7012`, `10ee:7021`, `10ee:7022`, `10ee:7024` and creates **`/dev/FPGA`**.
2. **Vendor-patched UHD 4.3–4.8** that links a closed-source static library:
   - `x64_libpcie.a` on `x86_64`
   - `arm_libpcie.a` on `aarch64` / ARM64

That library talks to `/dev/FPGA` and **replaces USB B210 discovery**. The vendor states that after you install this UHD, **USB B210 devices are no longer supported** by it.

Architectures the vendor actually shipped: **Linux x86_64 and Linux aarch64** (Raspberry Pi, Orange Pi, NanoPC, etc.). There is no macOS kext/dext and no Darwin `libpcie.a`.

---

## Quick verification (this is what “working” looks like)

PCIe:

```text
lspci -nnk | grep -A5 '10ee:7022'
# Memory controller [0580]: Xilinx Corporation Device [10ee:7022]
# Kernel driver in use: FPGA_PCIE
# Kernel modules: mymodule
```

Character device:

```text
ls -l /dev/FPGA
# crw-rw-rw- ... /dev/FPGA
```

UHD (must be the vendor-patched build, not distro UHD):

```text
uhd_config_info --version    # expect 4.8.0.0 (or whichever vendor tree you built)
uhd_find_devices
```

Successful find:

```text
[INFO] [UHD] linux; GNU C++ version ...; UHD_4.8.0.0-0-unknown
--------------------------------------------------
-- UHD Device 0
--------------------------------------------------
Device Address:
    serial: 191272
    name: MyB210
    type: b200
```

`sudo uhd_usrp_probe` should detect a B210, pass register loopback, and (on boards with the GPSDO) print something like `Found an internal GPSDO: ... for PCIEB210`.

---

## 1. Confirm the card is on the PCIe bus

Insert the M.2 **2280 M-key** module, then:

```bash
lspci | grep -i xil
# expect: ... Xilinx ... 7022   (2-lane bitstream; 7021 = 1-lane, 7024 = 4-lane)
```

If nothing appears:

- Reseat the card; power-cycle the machine (a live `pci rescan` is not enough on some carriers).
- Try another M.2 slot.
- On Raspberry Pi 5, see [docs/raspberry-pi-5.md](docs/raspberry-pi-5.md).

Blue **DONE** LED (LD10) on means the FPGA bitstream is loaded.

---

## 2. Install build dependencies

### Debian / Ubuntu / Raspberry Pi OS

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential dkms linux-headers-$(uname -r) \
  cmake pkg-config git unzip \
  libboost-all-dev libusb-1.0-0-dev libudev-dev \
  python3-dev python3-mako python3-numpy python3-requests python3-setuptools \
  python3-ruamel.yaml
```

If `linux-headers-$(uname -r)` is missing, install the headers package that matches `uname -r` (on Raspberry Pi OS that is typically `linux-headers-rpi-2712` and/or `linux-headers-rpi-v8`).

The vendor’s kitchen-sink script is in `scripts/vendor-apt_update.sh`. It installs a lot of GNU Radio / Qt extras you do **not** need just to get UHD talking to the card.

### Fedora / RHEL

```bash
sudo dnf install -y \
  gcc gcc-c++ make cmake dkms kernel-devel unzip \
  boost-devel libusb1-devel systemd-devel \
  python3-devel python3-mako python3-numpy python3-requests python3-setuptools
```

### Arch Linux

```bash
sudo pacman -S --needed \
  base-devel cmake dkms linux-headers unzip \
  boost libusb python python-mako python-numpy python-requests python-setuptools
```

---

## 3. Build and load the PCIe driver

From a clone of this repo:

```bash
cd pcie-driver
make
sudo ./load_module.sh          # insmod + mknod /dev/FPGA mode 666
ls /dev/FPGA
lspci -nnk | grep -A3 10ee
```

`load_module.sh` also accepts `unload` / `reload`.

### Survive reboot (DKMS + udev)

This is what was used on the reference Pi 5:

```bash
sudo mkdir -p /usr/src/m2sdr-0.25
sudo cp mymodule.c Makefile dkms.conf /usr/src/m2sdr-0.25/
sudo dkms add -m m2sdr -v 0.25
sudo dkms build -m m2sdr -v 0.25
sudo dkms install -m m2sdr -v 0.25
sudo cp ../udev/99-m2sdr.rules /etc/udev/rules.d/
sudo cp ../modules-load.d/m2sdr.conf /etc/modules-load.d/
sudo udevadm control --reload-rules
sudo modprobe mymodule
```

Or run `scripts/install-driver.sh`.

### Ubuntu 22.04+ / modern kernels

The vendor note (garbled encoding in the original 说明.txt) is: on Ubuntu 22 and newer, do **not** define `LO_KER` in `mymodule.c`. The copy in this repo already leaves `LO_KER` undefined, so the probe path uses `dma_set_mask_and_coherent(..., DMA_BIT_MASK(32))`.

The module also has `#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)` / `6,12,0` branches for `vm_flags_set` and `class_create`.

### Optional DMA-allocation fixes

`mymodule.c` in this repo includes two small fixes versus the vendor zip (see `pcie-driver/dma-fixes.patch`):

- `dma_free_coherent` is called with `phys << 3` because the driver stores the bus address shifted down by 3 bits.
- `dma_alloc_coherent` uses the requested length instead of a hardcoded 4096-byte buffer.

Discovery on the reference host worked with the **unpatched** vendor source via DKMS. Keep the patch if you rebuild from this tree.

---

## 4. Build vendor-patched UHD

**Do not** overlay these files onto a random Ettus git snapshot and expect it to work. Use the matching vendor zip (recommended: **4.8.0.0**, which is what enumerated MyB210 here).

```bash
# From the repo root
unzip vendor/uhd-4.8.0.0.zip -d /tmp
cd /tmp/uhd-4.8.0.0/host
# Extract the *full* zip (host + images + mpm). CMake failed when only host/ was unpacked.

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j"$(nproc)"
sudo make install
sudo ldconfig
```

`host/install_uhd.sh` is the vendor’s equivalent (it also deletes `./build` afterwards and runs `uhd_find_devices` / `uhd_usrp_probe`).

CMake picks the static library from `host/lib/usrp/b200/`:

```cmake
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(PCIE_LIB_PATH ".../x64_libpcie.a")
elif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|arm")
    set(PCIE_LIB_PATH ".../arm_libpcie.a")
endif()
link_libraries(${PCIE_LIB_PATH})
link_libraries(pthread)
```

If you already have Ettus UHD 4.1 (or any other UHD) in `/usr/local`, this **replaces** it. Put the vendor build in a prefix such as `/opt/m2sdr-uhd` if you still need USB B210s:

```bash
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/m2sdr-uhd
# then: export PATH=/opt/m2sdr-uhd/bin:$PATH
#        export LD_LIBRARY_PATH=/opt/m2sdr-uhd/lib:$LD_LIBRARY_PATH
```

On the reference host the vendor tree was installed to `/usr/local` and replaced UHD 4.1.

### Which UHD version?

| Zip | Notes |
|---|---|
| `vendor/uhd-4.8.0.0.zip` | Built and verified for `uhd_find_devices` / probe |
| `uhd-4.3.0.0.zip` … `uhd-4.7.0.0.zip` | Same vendor glue; not rebuilt on this host |

You cannot mix this kernel module with **stock** Ettus UHD 4.9 / 4.10 / main. The closed-source `libpcie` objects are only in these vendor trees.

---

## 5. Use the radio

```bash
sudo uhd_find_devices
sudo uhd_usrp_probe
```

GNU Radio, Gqrx, SDR++ etc. will see it only if they load **this** `libuhd.so` (same prefix). A distro `gnuradio` package linked against distro UHD will not.

Example RX (after streaming works on your kernel — see below):

```bash
/usr/local/lib/uhd/examples/rx_samples_to_file \
  --args "type=b200" --nsamps 20000 --rate 1e6 --freq 100e6 --gain 40 --file /tmp/m2sdr_rx.dat
```

---

## Raspberry Pi 5 extras

The FPGA BARs are 32-bit. On a Pi 5 the following was required in `/boot/firmware/config.txt`:

```ini
[all]
dtoverlay=pciex1-compat-pi5,l1ss=off,no-l0s=on,no-mip=off
dtoverlay=pcie-32bit-dma-pi5
dtparam=pciex1
#dtparam=pciex1_gen3
```

and in `/boot/firmware/cmdline.txt`:

```text
pci=noaer pcie_aspm=off
```

See [docs/raspberry-pi-5.md](docs/raspberry-pi-5.md) for the 16 KiB page / streaming issue.

---

## macOS

**Not supported with the files the vendor shipped.**

Reasons:

- The driver is a Linux `out-of-tree` kernel module (`mymodule.c`). There is no DriverKit / kext source.
- The UHD glue is two Linux static libraries (`arm_libpcie.a` is Linux aarch64 ELF; `x64_libpcie.a` is Linux x86_64 ELF). They will not link on Darwin.
- Consumer Macs do not expose a general-purpose M.2 NVMe slot to third-party PCIe endpoints. Thunderbolt enclosures would still need a macOS driver.

Use Linux (x86_64 or aarch64) on a machine with an M.2 M-key PCIe slot, or a Pi 5 / RK3588-class ARM SBC.

---

## Known issues

### `dma buff MMAP failed` then `Timeout while streaming`

Seen on Raspberry Pi 5 with **16 KiB** `PAGE_SIZE`. `uhd_find_devices` and register loopback succeed; RX/TX does not.

Cause: `arm_libpcie.a` calls `mmap(..., length=0x1000, offset=0)` and related calls with **hardcoded 4 KiB** lengths/offsets. Linux `mmap` offsets must be a multiple of `PAGE_SIZE`, so 4096 is invalid on a 16 KiB kernel.

Intended workaround (not verified after reboot on the reference host): boot the Pi 5 **4 KiB** kernel:

```ini
# /boot/firmware/config.txt  [all]
kernel=kernel8.img
```

then rebuild the DKMS module against `linux-headers-…-rpi-v8` so `uname -r` matches. Confirm with `getconf PAGESIZE` → `4096`.

x86_64 Linux (4 KiB pages) is what the vendor primarily tested; streaming is expected to work there. Ubuntu 18.04/20.04/22.04/24.04/25.04 are the platforms they ship prebuilt `.ko` files for; this repo always builds from source.

### This UHD build drops USB B210

Per vendor `UHD-mode/instructions.txt`. Keep a separate Ettus UHD prefix if you still use USB B2xx.

### Xilinx XDMA will not bind

`10ee:7022` is the stock XDMA ID, but this FPGA is a custom 4 KiB AXI-Lite endpoint, not XDMA. Official `xdma.ko` fails with `Failed to detect XDMA config BAR`.

---

## Hardware I/O (from the vendor handbook)

| Port | Function |
|---|---|
| TXA / TXB | Transmit A / B |
| RXA / RXB | Receive A / B (do not exceed 0 dBm) |
| PPS | PPS input |
| M.2 | PCIe |
| 40 MHz out | ~300 mV peak-to-peak |
| Ext clock in | UHD default 10 MHz |
| GPS | Active GPS antenna (GPSDO) |

LEDs: LD1 TXB (red), LD2 RXB (green), LD3 RXA (green), LD4 TXA (red), LD10 DONE (blue). LD5–LD9 unused in the handbook.

---

## Repository layout

```text
pcie-driver/          Linux kernel module (GPL), DKMS, load script
udev/                 /dev/FPGA mode 666
modules-load.d/       autoload mymodule
uhd-overlay/          vendor UHD glue copied out of uhd-4.8.0.0 (including libpcie.a)
vendor/               original HamGeek zips (driver + UHD 4.3–4.8)
scripts/              install helpers
docs/                 Pi 5 notes, hardware notes
```

The fastest path is: build `pcie-driver`, unpack `vendor/uhd-4.8.0.0.zip`, run CMake as above.

`uhd-overlay/` is the subset of files that differ from stock Ettus UHD 4.8 (B200 CMake, `b200_impl.cpp` discovery named MyB210, libusb zero-copy hooks, `arm_libpcie.a` / `x64_libpcie.a`). Prefer the full vendor zip.

---

## Credits and license

- Kernel module author: TQTT Liwei (GPL). Vendor contact in the original package: hamgeek@163.com.
- UHD host code is Ettus Research UHD (GPL-3.0-or-later) with vendor modifications.
- `arm_libpcie.a` / `x64_libpcie.a` are **closed-source vendor binaries**. Redistributed here only as part of the after-sales package so other owners of the same hardware can rebuild. See [NOTICE.md](NOTICE.md).

This is an unofficial community reconstruction of one successful Linux install. It is not affiliated with Ettus/NI.
