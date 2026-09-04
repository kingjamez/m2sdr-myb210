# HamGeek M2SDR (UHD name: MyB210)

Community install tree for the HamGeek **M2SDR**: an M.2 2280 PCIe SDR (Xilinx XC7A200 + AD9361) that enumerates in UHD as a B210 clone named **MyB210**.

This is **not** Enjoy Digital LiteX-M2SDR (`10ee:7024` / `m2sdr.ko` / SoapySDR). HamGeek’s card is a vendor-patched UHD B210 over a custom PCIe endpoint (`10ee:7021`/`7022`/`7024`).

Unofficial. Not affiliated with Ettus/NI or HamGeek. Vendor contact on the after-sales package: `hamgeek@163.com`.

**Streaming is verified** on a Raspberry Pi 5 (4 KiB pages, community DMA-pool driver, vendor UHD 4.8.0.0). With Ettus-sized USB frames (`recv_frame_size=8176,num_recv_frames=64`) **SDR++ is solid at 16–20 MS/s**. 24+ dies in seconds. 32–44 MS/s can look clean for a 4 s UHD burst and then fall over. Details: [docs/sample-rate.md](docs/sample-rate.md).

---

## What you need

Stock Ettus UHD and distro `uhd` packages **will not** see this card.

1. Linux kernel module `mymodule` (`FPGA_PCIE`) → **`/dev/FPGA`**
2. Vendor-patched UHD 4.3–4.8 linked against `arm_libpcie.a` (aarch64) or `x64_libpcie.a` (x86_64)

| Host | Status |
|---|---|
| Linux **x86_64**, 4 KiB pages | Vendor’s primary target |
| Linux **aarch64**, 4 KiB pages (Pi 5 `kernel8.img`) | Verified streaming |
| Pi 5 default **16 KiB** kernel | Discovery works. **Streaming does not**. Boot `kernel8.img`. |
| macOS / Windows | **Not supported** |

## Quick start

```bash
git clone https://github.com/kingjamez/m2sdr-myb210.git
cd m2sdr-myb210
./scripts/install-deps-debian.sh
./scripts/install-driver.sh
./scripts/install-uhd.sh
./scripts/verify-rx.sh
```

On a **Raspberry Pi 5**, do the [Pi 5 extra steps](docs/raspberry-pi-5.md) **before** the driver (4 KiB kernel, 32-bit DMA overlay).

Device args for every UHD tool:

```text
type=b200,recv_frame_size=8176,num_recv_frames=64
```

HamGeek’s default 3088-byte frames collapse at 16 MS/s. 8176/64 is what makes **16 and 20 MS/s** last.

## Use the radio

```bash
uhd_find_devices
/usr/local/lib/uhd/examples/rx_samples_to_file \
  --args "type=b200,recv_frame_size=8176,num_recv_frames=64" \
  --nsamps 20000 --rate 1e6 --freq 100e6 --gain 40 --file /tmp/m2sdr_rx.dat
```

SDR++: rebuild with `-DOPT_BUILD_USRP_SOURCE=ON`, apply [patches/sdrpp-usrp-source-myb210.patch](patches/sdrpp-usrp-source-myb210.patch). Source name is **USRP**. On a Pi 5 use **16 or 20 MS/s**, FFT **8192**. Fully quit after replacing the plugin.

See [docs/applications.md](docs/applications.md) and [docs/sample-rate.md](docs/sample-rate.md).

## Known issues

| Symptom | Fix |
|---|---|
| `dma buff MMAP failed` | 4 KiB kernel (`kernel8.img` on Pi 5) |
| `Timeout while streaming` | Community DMA pool at `0x40000000`. Never `cma=64M@1024M`. |
| Oops / FPGA D3cold | Never `rmmod mymodule` after RX. Reboot. |
| 16 MS/s timeouts | `recv_frame_size=8176,num_recv_frames=64` |
| 24 / 32 MS/s dies after a few seconds | Stay at **16–20 MS/s**. HamGeek must fix `libpcie`. |
| SDR++ crash on USRP select / 50 dB bowl | Apply the USRP source patch |

Vendor punch list: **[VENDOR-FEEDBACK.md](VENDOR-FEEDBACK.md)**.

`getconf PAGESIZE` must be 4096. Do not `rmmod` after RX. Do not relocate CMA.
