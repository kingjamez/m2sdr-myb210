# Sample rate

**Update 2026-09-30: the "20 MS/s ceiling" was a bug in vendor `libpcie`, and it is fixed.** With `scripts/patch-libpcie.py` (applied by `install-uhd.sh`), an x86_64 host streams **56 MS/s on one channel** and **30.72 MS/s on both channels**, lossless in `benchmark_rate`. See [Root cause and fix](#root-cause-and-fix-2026-09-30) below. The Raspberry Pi 5 sections after it are the earlier investigation; the Pi has **not** been re-tested with the fix yet.

---

## Root cause and fix (2026-09-30)

`libpcie` runs a `do_cb` thread that drains the FPGA's DMA completion FIFO. It reads the pending count from BAR0 register `0x1c` and masks it with **`0x3f`**. The card raises one MSI per packet (~27,500/s at 20 MS/s with the default 3088-byte frames). If `do_cb` is briefly starved and 64 or more completions pile up, the masked count reads wrong (64 → 0). `do_cb` stops draining, every DMA descriptor is used up, the FPGA stops interrupting, and **RX data and control replies (same FIFO) both stop**. UHD then reports `ERROR_CODE_TIMEOUT` and asserts in `wait_for_ack`.

Watched live by polling register `0x1c` from a second process during a 20 MS/s run: the backlog jumped to `0x53` (83) at 7.0 s, MSIs dropped to zero, and it settled at `0x40`. (Reading register `0x1d` **pops** the FIFO; only peek `0x1c`.)

The fix widens the mask to `0x1ff`: one instruction in `x64_libpcie.a` and in `arm_libpcie.a` (same library in vendor UHD 4.3–4.8). Community driver 0.27 also makes the completion wait re-poll after 2 ms instead of stalling on a missed MSI. See [pcie-driver/COMMUNITY-CHANGES.md](../pcie-driver/COMMUNITY-CHANGES.md).

### Frame size after the fix

Larger frames mean fewer interrupts, syscalls and thread handoffs per second. Measured on x86_64 at 56 MS/s, 1 channel:

| `recv_frame_size` | Samples/packet | Result |
|---|---|---|
| 3088 (vendor default) | 768 | overflows at high rates (~73k packets/s) |
| 4104 | 1022 | overflows |
| 8176 | 2040 | clean 10 s; 8 overruns in 60 s |
| **12272** | **3064** | **clean** (see below) |
| 16360 | 4086 | ~1% sequence errors, `bad vrt header` bursts (FPGA packet limit < 16 KiB) |

Use **`recv_frame_size=12272`**. To apply it to every UHD program, create `/etc/uhd/uhd.conf`:

```ini
[type=b200]
recv_frame_size=12272
```

Explicit device args still override it.

`num_recv_frames` **is ignored**: vendor `b200_impl.cpp` overwrites it with 48 (`data_xport_args["num_recv_frames"] ="48"`), so earlier 32-vs-64 comparisons below measured noise. 48 frames × 3064 samples is ~2.6 ms of buffering at 56 MS/s, which is the source of the rare overruns that remain. Raising it needs a UHD rebuild, and must stay below 64 (`libpcie` has 64 slots per channel and calls `exit(1)` when all are busy).

### Measured (x86_64, fix applied)

Host: UGREEN DXP2800 (Intel N100, 4 cores), Ubuntu 24.04, kernel 7.0, IOMMU on, card at PCIe 5 GT/s x2, CPU governor `performance`. `benchmark_rate`, `recv_frame_size=12272`, master clock = sample rate.

| Config | Duration | Drops / overruns | Seq errors |
|---|---|---|---|
| 1 ch 20 MS/s | 30 s | 0 / 0 | 0 |
| 1 ch 40 MS/s | 30 s | 0 / 0 | 0 |
| 1 ch 56 MS/s | 7 runs, 30–60 s | 6 runs 0 / 0; 1 run 2 overruns | 0 |
| 2 ch 16 / 25 / 30.72 MS/s | 30 s each | 0 / 0 | 0 |
| 2 ch 30.72 MS/s | 120 s | 1 overrun | 0 |

Before the fix, the same host stalled within 2–7 s at 20 MS/s every time. `benchmark_rate` at 56 MS/s uses ~31% of one core.

SDR++ itself is the limit well below 56 MS/s on a small CPU: its DSP chain overflows even though the transport is clean.

---

# Raspberry Pi 5 investigation (before the libpcie fix)

How the highest **usable** RX rate was found on the reference Pi host (Pi 5 + 52Pi EP-0180 + NVMe root + community `mymodule` + vendor UHD 4.8), **before** the `libpcie` fix above. The ceilings below describe the unpatched library.

The AD9361 will clock any of 32 / 40 / 44 / 48 / 50 / 56 / 61.44 MHz. That is not the limit.

**Unpatched sustained ceiling (SDR++ and long UHD runs): 20 MS/s.**  
**16 MS/s was the rock-solid daily rate.** 24+ died in a few seconds. 32–44 MS/s could look lossless for a **4 s** `benchmark_rate` and then fall over.

---

## Device args (the one change that mattered)

HamGeek’s default USB frame is **3088 bytes** (`24*32*4+16`). Ettus B210s use ~**8176**. On this transport, 3088 bytes dies at 16 MS/s. 8176/64 is what makes **16 and 20 MS/s** last. 4-second runs at 32–44 MS/s with the same args are not sustainable.

Use this on **every** UHD tool, including SDR++:

```text
type=b200,recv_frame_size=8176,num_recv_frames=64
```

```bash
/usr/local/lib/uhd/examples/benchmark_rate \
  --args "type=b200,recv_frame_size=8176,num_recv_frames=64" \
  --rx_rate 20e6 --duration 10

/usr/local/lib/uhd/examples/rx_samples_to_file \
  --args "type=b200,recv_frame_size=8176,num_recv_frames=64" \
  --rate 20e6 --freq 100e6 --gain 40 --nsamps 0 --duration 10 \
  --file /tmp/m2sdr_20msps.dat
```

Or `./scripts/benchmark-rate.sh` (`RATE=16e6` for the conservative check). Use `--duration 10` or more; 4 s hides the 32 MS/s collapse.

Do **not** use `recv_frame_size=16360` (Ettus `B200_USB_DATA_MAX_RECV_FRAME_SIZE`). On this card it produces `ERROR_CODE_BAD_PACKET` and sequence errors. Do **not** use `num_recv_frames=128`; UHD threw `uhd::assertion_error`.

UHD’s `get_rx_rates()` list still tops out around 16 MS/s (USB-era table). Ignore it. `set_rx_rate(40e6)` actually got a 40 MHz master clock.

---

## Measured results (this host)

NVMe root, FPGA trained **PCIe Gen2 5 GT/s x1** (bitstream advertises x2; the Pi 5 FFC and the ASM1184e uplink are x1). IQ is sc16 = 4 bytes/sample.

**Longer runs (what you can actually use):**

| Rate | Tool | Args | Duration | Result |
|---|---|---|---|---|
| 16 MS/s | `benchmark_rate` | 8176 / 64 | 10 s | lossless 160 M |
| 20 MS/s | `benchmark_rate` | 8176 / 64 | 8 s | lossless 160 M |
| 20 MS/s | **SDR++** | patched plugin | minutes | **usable ceiling** |
| 24 MS/s | `benchmark_rate` | 8176 / 64 | 8 s | lossless 192 M |
| 24 MS/s | **SDR++** | patched plugin | seconds | **dies** (DSP + `libpcie`) |
| 32 MS/s | `benchmark_rate` | 8176 / 32 | 8 s | 63 M of 256 M, 57 timeouts |
| 32 MS/s | `benchmark_rate` | 8176 / 64 | 8 s | 256 M, then later runs time out |
| 32 MS/s | **SDR++** | patched plugin | ~4 s | starts, then dies |

4-second `benchmark_rate` bursts (kept for history; **not** the ceiling):

| Rate | Args | Clock | Received / expected (4 s) | Drops | Overruns | RX timeouts | Verdict |
|---|---|---|---|---|---|---|---|
| 8 MS/s | HamGeek default (~3088 / 32) | 8 MHz | clean | 0 | 0 | 0 | OK, too slow to bother |
| 16 MS/s | default 3088 / 32 | 16 MHz | ~7.7 M / 64 M | yes | — | 32 | **fail** |
| 16 MS/s | **8176 / 32** | 16 MHz | 64.0 M | 0 | 0 | 0 | lossless |
| 16 MS/s | 16360 / 32 | 16 MHz | full count | — | — | — | 114 sequence errors |
| 32 MS/s | **8176 / 32** | 32 MHz | 128.1 M | 0 | 0 | 0 | lossless |
| 32 MS/s | 16360 / 32 | 32 MHz | 128.2 M | 330 | 0 | 0 | 330 sequence errors |
| 40 MS/s | 8176 / 32 | 40 MHz | 77.7 M / 160 M | 142 k | 1 | 18 | fail |
| 40 MS/s | 8176 / 64 | 40 MHz | 160.0 M | 0 | 0 | 0 | 4 s burst only |
| 44 MS/s | 8176 / 64 | 44 MHz | 177.0 M | 0 | 0 | 0 | 4 s burst only |
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

Apply these in order. 1–3 are required to stream at all. 4–5 are what moved the ceiling from 8 MS/s to **20 MS/s sustained**. 6–10 are quality / crash fixes, not throughput.

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

This is the largest single gain. Default 3088-byte frames are ~20k frames/s at 16 MS/s and collapse. Ettus-sized 8 KiB frames are what made **16 MS/s last**.

### 5. ~~`num_recv_frames=64`~~ (has no effect)

Vendor `b200_impl.cpp` forces `num_recv_frames` to 48 whatever you pass, so this argument does nothing. The 32-vs-64 differences in the tables were run-to-run variation.

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
- lists 1 / 2 / 4 / 8 / 16 / **20** MS/s
- analog BW = sample rate, DC/IQ auto, keep `usrp` alive, skip GPSDO poke
- drain overflows / kick the stream instead of freezing

Source name in the UI is **USRP**. Use **16 or 20 MHz**. Fully quit after installing the `.so`.

---

## What only HamGeek can fix

See [VENDOR-FEEDBACK.md](../VENDOR-FEEDBACK.md).

- Merge the `do_cb` count fix into `libpcie` (the community binary patch works around it).
- Default `recv_frame_size` 3088 → 12272 (or drop USB framing).
- Honour `num_recv_frames` instead of forcing 48.
- `mmap` with `PAGE_SIZE`, not hardcoded 4 KiB.
- Safe `rmmod` after RX.
- Unique PCI ID (not stock XDMA `10ee:7022`).
- FPGA x2 on a carrier that actually has two lanes to the card (this HAT will not).
- Gen3: FPGA max_link_speed is 5 GT/s; a bitstream change would be required.

Without the `libpcie` fix, **20 MS/s** was the Pi 5 + EP-0180 **SDR++** number and **16 MS/s** the conservative daily rate. With the fix, re-test on the Pi: the x86_64 results above are the only verified ones so far.
