# Sample rate on Raspberry Pi 5

How to get the highest **lossless** RX rate on the reference host (Pi 5 + 52Pi EP-0180 + NVMe root + community `mymodule` + vendor UHD 4.8). Measured 2026-09-04 with `benchmark_rate`, sc16, 4-second runs.

The AD9361 will clock any of 32 / 40 / 44 / 48 / 50 / 56 / 61.44 MHz. That is not the limit. The limit is vendor `libpcie` talking to the FPGA like a USB B210.

**Community lossless ceiling on this Pi: 44 MS/s.**  
**50 MS/s and 61.44 MS/s clock, then drop samples.** Those need HamGeek.

---

## Device args (the one change that mattered)

HamGeek’s default USB frame is **3088 bytes** (`24*32*4+16`). Ettus B210s use ~**8176**. On this transport, 3088 bytes dies at 16 MS/s. 8176 bytes is lossless at 32 MS/s (32 buffers) and at 40–44 MS/s (64 buffers).

Use this on **every** UHD tool, including SDR++:

```text
type=b200,recv_frame_size=8176,num_recv_frames=64
```

```bash
/usr/local/lib/uhd/examples/benchmark_rate \
  --args "type=b200,recv_frame_size=8176,num_recv_frames=64" \
  --rx_rate 40e6 --duration 4

/usr/local/lib/uhd/examples/rx_samples_to_file \
  --args "type=b200,recv_frame_size=8176,num_recv_frames=64" \
  --rate 40e6 --freq 100e6 --gain 40 --nsamps 0 --duration 4 \
  --file /tmp/m2sdr_40msps.dat
```

Or `./scripts/benchmark-rate.sh` (`RATE=44e6` to try the ceiling).

Do **not** use `recv_frame_size=16360` (Ettus `B200_USB_DATA_MAX_RECV_FRAME_SIZE`). On this card it produces `ERROR_CODE_BAD_PACKET` and sequence errors. Do **not** use `num_recv_frames=128`; UHD threw `uhd::assertion_error`.

UHD’s `get_rx_rates()` list still tops out around 16 MS/s (USB-era table). Ignore it. `set_rx_rate(40e6)` actually got a 40 MHz master clock.

---

## Measured results (this host)

NVMe root, FPGA trained **PCIe Gen2 5 GT/s x1** (bitstream advertises x2; the Pi 5 FFC and the ASM1184e uplink are x1). IQ is sc16 = 4 bytes/sample.

| Rate | Args | Clock | Received / expected (4 s) | Drops | Overruns | RX timeouts | Verdict |
|---|---|---|---|---|---|---|---|
| 8 MS/s | HamGeek default (~3088 / 32) | 8 MHz | clean | 0 | 0 | 0 | OK, too slow to bother |
| 16 MS/s | default 3088 / 32 | 16 MHz | ~7.7 M / 64 M | yes | — | 32 | **fail** |
| 16 MS/s | **8176 / 32** | 16 MHz | 64.0 M | 0 | 0 | 0 | lossless |
| 16 MS/s | 16360 / 32 | 16 MHz | full count | — | — | — | 114 sequence errors |
| 32 MS/s | **8176 / 32** | 32 MHz | 128.1 M | 0 | 0 | 0 | lossless |
| 32 MS/s | 16360 / 32 | 32 MHz | 128.2 M | 330 | 0 | 0 | 330 sequence errors |
| 40 MS/s | 8176 / 32 | 40 MHz | 77.7 M / 160 M | 142 k | 1 | 18 | fail |
| **40 MS/s** | **8176 / 64** | **40 MHz** | **160.0 M** | **0** | **0** | **0** | **lossless** |
| **44 MS/s** | **8176 / 64** | **44 MHz** | **177.0 M** | **0** | **0** | **0** | **lossless (ceiling)** |
| 48 MS/s | 8176 / 64 | 48 MHz | 92.1 M / 192 M | 527 k | 3 | 18 | fail |
| 50 MS/s | 8176 / 32 or /64 | 50 MHz | 112.9 M / 200 M (at /64) | 516 k | 4 | 16 | fail |
| 61.44 MS/s | 8176 / 32 | 61.44 MHz | 70.3 M / 246 M | 0 counted | 0 | 26 | fail (timeouts) |
| 61.44 MS/s | 8176 / 64 | 61.44 MHz | 240.9 M / 245.8 M | 1.51 M | 7 | 1 | ~98%, not lossless |
| 61.44 MS/s | 8176 / 128 | 61.44 MHz | — | — | — | — | `uhd::assertion_error` |
| 61.44 MS/s | 16360 / 64 | 61.44 MHz | 71.3 M | 82 M | 0 | 27 | BAD_PACKET |

Failed high-rate runs often abort at teardown with `boost::lock_error` in vendor `my_c2h_buf_cb`. Reboot if the next open hangs. Never `rmmod mymodule`.

Wire math (why this is not a PCIe-average problem):

| Rate | sc16 payload | vs Gen2 x1 (~500 MB/s theoretical) |
|---|---|---|
| 32 MS/s | 128 MB/s | ~26% |
| 40 MS/s | 160 MB/s | ~32% |
| 44 MS/s | 176 MB/s | ~35% |
| 50 MS/s | 200 MB/s | ~40% |
| 61.44 MS/s | 246 MB/s | ~49% |

Average link bandwidth is enough for 61.44. The stall is USB-style frame rate and `wait_for_ack` inside closed `libpcie`.

---

## Optimizations that raised the usable rate

Apply these in order. 1–3 are required to stream at all. 4–5 are what moved the ceiling from 8 MS/s to 44 MS/s. 6–10 are quality / crash fixes, not throughput.

### 1. 4 KiB pages (`kernel8.img`)

Vendor `arm_libpcie.a` `mmap()`s with hardcoded 4096-byte lengths/offsets. Default Pi 5 16 KiB kernels (`*-rpi-2712`) print `dma buff MMAP failed` and time out. Discovery still works.

```bash
getconf PAGESIZE    # must be 4096
```

See [raspberry-pi-5.md](raspberry-pi-5.md).

### 2. 32-bit DMA overlay + no ASPM

`/boot/firmware/config.txt`:

```ini
kernel=kernel8.img
dtoverlay=pciex1-compat-pi5,l1ss=off,no-l0s=on,no-mip=off
dtoverlay=pcie-32bit-dma-pi5
dtparam=pciex1
```

cmdline: `pci=noaer pcie_aspm=off`. FPGA BARs/DMA are 32-bit; the card misbehaves with L1SS/L0s.

### 3. Community DMA pool (not CMA relocate)

Vendor `dma_alloc_coherent` lands in the Pi 5 64 MiB CMA hole (`0x3b800000–0x3f7fffff`). IRQs can fire; IQ is zeros / `wait_for_ack`.

`cma=64M@1024M` can make RX work **and kills HDMI**. Do not use it.

This repo’s `pcie-driver/mymodule.c` carves a 16 MiB pool at **`0x40000000`** (`DMA_BIT_MASK(31)`, 2 GiB inbound window) and leaves default CMA for `vc4`. `dmesg` should show `1GiB pool … dma=0x0000000040000000` tagged `pool`.

### 4. `recv_frame_size=8176`

This is the largest single gain. Default 3088-byte frames are ~20k frames/s at 16 MS/s and collapse. Ettus-sized 8 KiB frames cut that rate in half and made **32 MS/s lossless**.

### 5. `num_recv_frames=64`

32 buffers are enough through 32 MS/s. 40 MS/s with 32 buffers overruns; 64 buffers are lossless at 40 and 44 MS/s. 128 buffers assert. Stay at 64.

### 6. Analog RX bandwidth = sample rate

Stock SDR++ “Auto” programmed the AD9361 analog BB LPF to its **200 kHz minimum**. At 1 MS/s that is a ~50 dB bowl (hot center, dead edges) plus fake spurs. Set analog BW equal to the sample rate (`set_rx_bandwidth(rate)`). Does not change MS/s; it makes the spectrum usable at whatever rate you picked.

### 7. DC-offset and IQ-balance auto-cal

`set_rx_dc_offset(true)` and `set_rx_iq_balance(true)` after tune. Without them you get a DC spike and an IQ image that look like signals.

### 8. Keep the `usrp` object alive

Vendor `libpcie` starts C2H callbacks in `multi_usrp::make()`. Destroying a local `shared_ptr` (stock SDR++ `select()`, or `dev.reset()` on Stop) aborts in `boost::lock_error` (`libusb_async_cb` / `my_c2h_buf_cb`). Keep one device object until process exit.

### 9. Do not poke the GPSDO clock source on every Play

`set_clock_source()` re-inits the GPSTCXO UART and has timed out (`fifo ctrl timed out`). Leave the clock `make()` chose (internal) unless the user actually changes it.

### 10. SDR++ FFT 8192; fully quit after replacing the plugin

65536-point FFT plus a high sample rate has bus-errored this Pi 5 GPU. After copying `usrp_source.so`, **fully quit** SDR++ — Stop is not enough.

---

## Things that did **not** raise the rate

| Tried | Result |
|---|---|
| Boot from SD so NVMe is idle | FPGA still trains **5 GT/s x1**. 8 MS/s was already lossless with NVMe root. Not the 16 MS/s limiter. |
| `recv_frame_size=16360` | Sequence errors / `BAD_PACKET`. Too big for this USB-emulation path. |
| `num_recv_frames=128` | `uhd::assertion_error`. |
| `cma=64M@1024M` | RX can work; HDMI dies. |
| Enable Pi 5 Gen3 (`dtparam=pciex1_gen3`) | Switch is Gen2; FPGA max is 5 GT/s. |
| Expect FPGA x2 on EP-0180 | Bitstream advertises x2; FFC + ASM1184e uplink are x1. |

---

## SDR++

Rebuild with `-DOPT_BUILD_USRP_SOURCE=ON` against vendor UHD 4.8, apply [patches/sdrpp-usrp-source-myb210.patch](../patches/sdrpp-usrp-source-myb210.patch). The patch:

- injects `recv_frame_size=8176,num_recv_frames=64` on `make()`
- lists 1 / 2 / 4 / 8 / 16 / 32 / **40 / 44** MS/s
- analog BW = sample rate, DC/IQ auto, keep `usrp` alive, skip GPSDO poke

Source name in the UI is **USRP**. Use **40 MHz** or **44 MHz**, not 50. Fully quit after installing the `.so`.

---

## What only HamGeek can fix (50–61.44 MS/s)

These are not community-tunable. See [VENDOR-FEEDBACK.md](../VENDOR-FEEDBACK.md).

- Default `recv_frame_size` 3088 → Ettus 8176 (or drop USB framing).
- Open `libpcie` (or ship a `.so`) so C2H can use large DMA buffers instead of USB packets and `wait_for_ack`.
- `mmap` with `PAGE_SIZE`, not hardcoded 4 KiB.
- Safe `rmmod` after RX.
- Unique PCI ID (not stock XDMA `10ee:7022`).
- FPGA x2 on a carrier that actually has two lanes to the card (this HAT will not).
- Gen3: FPGA max_link_speed is 5 GT/s; a bitstream change would be required.

Until then, treat **44 MS/s** as the Pi 5 + EP-0180 number, **40 MS/s** as the conservative daily rate.
