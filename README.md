# HamGeek M2SDR (UHD name: MyB210)

Community install tree for the HamGeek **M2SDR**: an M.2 2280 PCIe SDR (Xilinx XC7A200 + AD9361) that enumerates in UHD as a B210 clone named **MyB210**.

This is **not** Enjoy Digital LiteX-M2SDR (`10ee:7024` / `m2sdr.ko` / SoapySDR). HamGeek’s card is a vendor-patched UHD B210 over a custom PCIe endpoint (`10ee:7021`/`7022`/`7024`).

Unofficial. Not affiliated with Ettus/NI or HamGeek. Vendor contact on the after-sales package: `hamgeek@163.com`.

**Streaming is verified** on a Raspberry Pi 5 (4 KiB pages, community DMA-pool driver, vendor UHD 4.8.0.0): `rx_samples_to_file` wrote 20 000 sc16 samples, SDR++ USRP source opened the card as a B210.

---

## What you need

Two pieces. Stock Ettus UHD and distro `uhd` packages **will not** see this card.

1. Linux kernel module `mymodule` (`FPGA_PCIE`) → **`/dev/FPGA`**
2. Vendor-patched UHD 4.3–4.8 linked against a closed-source static library:
   - `x64_libpcie.a` on `x86_64`
   - `arm_libpcie.a` on `aarch64`

That library talks to `/dev/FPGA` and **replaces USB B210 discovery**. After you install this UHD into a prefix, **USB B210s are no longer supported** by that prefix.

### Supported hosts

| Host | Status |
|---|---|
| Linux **x86_64**, 4 KiB pages, M.2 M-key PCIe slot | Vendor’s primary target. Use this tree. |
| Linux **aarch64**, 4 KiB pages (Pi 5 `kernel8.img`, many SBCs) | Verified streaming on Pi 5 with the community driver in `pcie-driver/mymodule.c`. |
| Raspberry Pi 5 default **16 KiB** kernel (`*-rpi-2712`) | Discovery works. **Streaming does not** (`dma buff MMAP failed`). Boot `kernel8.img`. |
| macOS / Windows | **Not supported.** No kext/dext/INF, and `libpcie.a` is Linux ELF only. |

You need a machine that exposes a general-purpose M.2 **M-key** PCIe slot (not NVMe-only firmware). Blue **DONE** LED (LD10) on the card means the FPGA bitstream is loaded.

---

## Quick start (any Linux x86_64 / aarch64 with 4 KiB pages)

```bash
git clone https://github.com/kingjamez/m2sdr-myb210.git
cd m2sdr-myb210

# 1. Kernel headers + build tools
./scripts/install-deps-debian.sh    # or install-deps-fedora.sh / install-deps-arch.sh

# 2. Driver (DKMS, udev, autoload). Reboot if the script says so — do not rmmod after RX.
./scripts/install-driver.sh

# 3. Vendor UHD 4.8 into /usr/local  (replaces any UHD already there)
# If vendor/uhd-4.8.0.0.zip is missing, copy it from the HamGeek after-sales package.
./scripts/install-uhd.sh
# Safer if you still use USB B210s:
#   PREFIX=/opt/m2sdr-uhd ./scripts/install-uhd.sh

# 4. Prove streaming
./scripts/verify-rx.sh
```

Success looks like:

```text
uhd_find_devices  →  name: MyB210  type: b200
rx_exit=0
/tmp/m2sdr_rx.dat  is non-zero (20000 sc16 samples ≈ 80000 bytes)
```

Device args for every UHD tool:

```text
type=b200
# or serial=<your serial>   (reference card: 191272)
```

On a **Raspberry Pi 5**, do the [Pi 5 extra steps](docs/raspberry-pi-5.md) **before** step 2 (4 KiB kernel, 32-bit DMA overlay). Then reboot and run the same driver/UHD/verify commands.

---

## 1. Confirm the card is on the PCIe bus

```bash
lspci -nnk | grep -A5 -E '10ee:70'
# Memory controller [0580]: Xilinx Corporation Device [10ee:7022]   # x2 bitstream
# Kernel driver in use: FPGA_PCIE
# Kernel modules: mymodule
```

| PCI ID | Vendor comment |
|---|---|
| `10ee:7021` | 1-lane bitstream |
| `10ee:7022` | 2-lane (reference card) |
| `10ee:7024` | 4-lane. **Different product from LiteX-M2SDR**, which also uses `7024`. |

If nothing appears: reseat, **power-cycle** (a live `pci rescan` is not enough on some carriers), try another M.2 slot.

---

## 2. Page size (do this first on Pi 5)

```bash
getconf PAGESIZE    # must be 4096 for streaming
```

Vendor `arm_libpcie.a` / `x64_libpcie.a` call `mmap()` with **hardcoded 4 KiB** lengths and offsets. Linux requires mmap offsets to be a multiple of `PAGE_SIZE`, so a 16 KiB-page kernel fails with:

```text
dma buff MMAP failed
Timeout while streaming
```

Discovery and register loopback still succeed. That is why `uhd_find_devices` is not a streaming test.

---

## 3. Build and load the PCIe driver

This repo’s `pcie-driver/mymodule.c` is the **community driver**: vendor r25 plus DMA fixes that were required for streaming on a Pi 5 (32-bit / 2 GiB inbound window, contiguous pool above the 1 GiB mark so HDMI CMA is left alone). `pcie-driver/mymodule.c.vendor` is the unmodified zip.

```bash
./scripts/install-driver.sh
ls -l /dev/FPGA
```

One-shot (no DKMS):

```bash
cd pcie-driver
make
sudo ./load_module.sh
```

**Do not `rmmod` / `modprobe -r` `mymodule` after any RX/TX attempt.** That Oopses in `free_node` and can leave the FPGA in D3cold. Reboot to recover. `install-driver.sh` will refuse to unload a live module and tell you to reboot.

---

## 4. Build vendor-patched UHD

Use the matching vendor zip. **4.8.0.0** is what enumerated MyB210 and streamed on the reference host.

```bash
./scripts/install-uhd.sh
# or:  PREFIX=/opt/m2sdr-uhd ./scripts/install-uhd.sh
```

Do **not** overlay these files onto stock Ettus 4.9 / 4.10 / main. The closed-source `libpcie` objects exist only in the vendor 4.3–4.8 trees.

Confirm you are calling **this** UHD:

```bash
uhd_config_info --version    # 4.8.0.0-0-unknown from /usr/local (or your PREFIX)
uhd_find_devices
```

```text
Device Address:
    serial: 191272
    name: MyB210
    type: b200
```

---

## 5. Use the radio

```bash
uhd_find_devices
uhd_usrp_probe --args "type=b200"
/usr/local/lib/uhd/examples/rx_samples_to_file \
  --args "type=b200" --nsamps 20000 --rate 1e6 --freq 100e6 --gain 40 \
  --file /tmp/m2sdr_rx.dat
```

Or `./scripts/verify-rx.sh`.

### GNU Radio, Gqrx, SDR++

They see the card **only** if they load this `libuhd.so` (same prefix). A distro package linked against distro/`libuhd.so.4.1` will not.

SDR++: rebuild with `-DOPT_BUILD_USRP_SOURCE=ON` against the vendor UHD, and apply [patches/sdrpp-usrp-source-myb210.patch](patches/sdrpp-usrp-source-myb210.patch). In the UI the source is named **USRP**. On a Pi 5 start at **1 MS/s**, FFT **8192**. Stock SDR++ “Auto” bandwidth programs the AD9361 analog filter to 200 kHz and looks like a 50 dB bowl; selecting USRP without the patch aborts in vendor `libpcie` callbacks.

See [docs/applications.md](docs/applications.md).

---

## Raspberry Pi 5

Required extras (32-bit DMA overlay, 4 KiB `kernel8.img`, no CMA relocate):

**[docs/raspberry-pi-5.md](docs/raspberry-pi-5.md)** and `scripts/setup-raspberry-pi5.sh`.

Do **not** put `cma=64M@1024M` on the cmdline. That moves all CMA to 1 GiB, streaming can work, **HDMI dies**.

---

## Known issues

| Symptom | Cause | Fix |
|---|---|
| `dma buff MMAP failed` | 16 KiB `PAGE_SIZE` | Boot a 4 KiB kernel (`kernel8.img` on Pi 5) |
| `Timeout while streaming` / `wait_for_ack`, IRQs may still fire | DMA buffers in the Pi 5 64 MiB CMA hole (`0x3b800000–0x3f7fffff`) | Use this community driver (pool at `0x40000000`). Do **not** relocate CMA. |
| Oops in `free_node`, FPGA D3cold | `rmmod mymodule` after RX | Reboot. Never live-unload. |
| `No UHD Devices Found` | Distro/Ettus UHD, or `/dev/FPGA` missing | Vendor 4.8 + `mymodule` loaded |
| Gqrx/GNU Radio silent | Linked to `libuhd.so.4.1.0` | Rebuild against vendor 4.8 |
| Official `xdma.ko` fails | `10ee:7022` is **not** XDMA | Use `mymodule` |
| SDR++ aborts when USRP is selected | Local `multi_usrp` destroyed while `libpcie` C2H callbacks run | Apply `patches/sdrpp-usrp-source-myb210.patch` |
| Waterfall 50 dB bowl, extra spurs | Analog RX BW left at AD9361 200 kHz min | Same patch: set analog BW = sample rate; enable DC/IQ auto |

Requested vendor changes for the next software drop: **[VENDOR-FEEDBACK.md](VENDOR-FEEDBACK.md)**.

---

## Hardware I/O

See [docs/hardware.md](docs/hardware.md). RX ports: do not exceed 0 dBm. Ext clock default 10 MHz. GPSDO present on the reference board.

---

## Repository layout

```text
pcie-driver/          Community kernel module (GPL) + vendor original
udev/                 /dev/FPGA mode 666
modules-load.d/       autoload mymodule
uhd-overlay/          vendor UHD glue (libpcie.a, MyB210 discovery)
vendor/               original HamGeek zips (driver r25 + UHD 4.3–4.8)
scripts/              deps, driver, UHD, Pi 5, RX verify
docs/                 Pi 5, generic Linux, apps, hardware
patches/              SDR++ USRP source patch for MyB210
VENDOR-FEEDBACK.md    notes for HamGeek’s next revision
```

`uhd-overlay/` is the subset that differs from stock Ettus UHD 4.8. Prefer building the full `vendor/uhd-4.8.0.0.zip`.

---

## Credits and license

- Kernel module author: TQTT Liwei (`MODULE_LICENSE("GPL")`).
- UHD host code: Ettus Research UHD, GPL-3.0-or-later, plus vendor modifications.
- `arm_libpcie.a` / `x64_libpcie.a` are **closed-source vendor binaries**. Redistributed only so owners of the same hardware can rebuild. See [NOTICE.md](NOTICE.md).

If you are the vendor and object to redistribution of the blobs, open an issue and they can be replaced with a download-from-vendor step.
