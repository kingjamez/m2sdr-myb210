# Feedback for HamGeek — next M2SDR / MyB210 software revision

**To:** HamGeek after-sales (`hamgeek@163.com`)  
**From:** community bring-up of M2SDR (UHD name **MyB210**) on Raspberry Pi 5 + 52Pi EP-0180  
**Package tested:** `b210_model_pcie_drv_r25` + vendor UHD **4.8.0.0**, last vendor stamp 2026-05-27  
**Date:** 2026-09-04

This is engineering feedback from a working install. Discovery worked quickly. Streaming on Raspberry Pi 5 did not, until the kernel module and boot config were changed as below. The closed-source `arm_libpcie.a` is the main blocker for a clean upstream-quality driver.

Please treat this as a punch list for the next zip you ship to customers.

---

## Reference setup (what actually streamed)

| Item | Value |
|---|---|
| Host | Raspberry Pi 5 Model B Rev 1.1, Debian 13, 16 GiB |
| Kernel that streams | `kernel8.img` / `6.12.47+rpt-rpi-v8`, **PAGE_SIZE=4096** |
| Kernel that does not stream | default `kernel_2712.img` / `*-rpi-2712`, **PAGE_SIZE=16384** |
| Carrier | 52Pi EP-0180, ASM1184e PCIe Gen2 x1 switch |
| Card | `10ee:7022` subsystem `10ee:0007`, `/dev/FPGA`, serial **191272** |
| Coexistence | NVMe OS disk on the same switch |
| Proof | `rx_samples_to_file --args type=b200 --nsamps 20000 --rate 1e6` → `rx_exit=0`, 80000-byte file |
| Sustained RX ceiling | **20 MS/s** in SDR++ / long UHD runs. 16 MS/s is rock-solid. 32–44 MS/s can look lossless for **4 s** then time out. |

HDMI stayed up. Relocating CMA was **not** used in the working configuration.

---

## What already works (keep this)

- FPGA enumerates, BAR access, register loopback, LO lock, GPSDO (`GPSTCXO v3.8 for PCIEB210`).
- `uhd_find_devices` reports `name: MyB210`, `type: b200`.
- DKMS-friendly out-of-tree module with `LINUX_VERSION_CODE` branches for `vm_flags_set` / `class_create`.
- Leaving `LO_KER` undefined so Ubuntu 22+ uses `dma_set_mask_and_coherent`.

---

## Please fix in the next revision

### 1. `libpcie.a`: stop hardcoding 4 KiB in `mmap`

**Severity: blocker on any 16 KiB-page kernel (default Pi 5).**

`arm_libpcie.a` calls `mmap(..., length=0x1000, offset=0)` and related calls with hardcoded 4096. Linux requires mmap offsets to be a multiple of `PAGE_SIZE`. On Pi 5 16 KiB kernels this prints `dma buff MMAP failed` and UHD times out. Discovery still works because BAR0 is mapped at offset 0.

**Ask:** use `sysconf(_SC_PAGESIZE)` / `getpagesize()` for every mmap length and offset. Rebuild `arm_libpcie.a` and `x64_libpcie.a`. Until then, Pi 5 customers must boot `kernel8.img` (4 KiB), which is a support burden you should not impose.

### 2. Kernel driver: do not DMA into the Pi 5 CMA hole

**Severity: blocker for streaming on Pi 5 even with 4 KiB pages.**

Default CMA is 64 MiB at `0x3b800000–0x3f7fffff` (`/proc/iomem` `reserved`). Vendor `dma_alloc_coherent` returns addresses like `0x3bc18000` inside that hole. The FPGA writes there; userspace mmaps other pages; IRQs may fire; `rx_samples_to_file` returns `wait_for_ack` / timeout; the IQ file is empty.

`cma=64M@1024M` moves buffers to `0x40418000` and RX works, but **HDMI dies** (`vc4-kms-v3d` has no CMA). Do not document that cmdline.

**Ask:**

- Set the DMA mask to the host’s real inbound window. On Pi 5 that is **2 GiB** (`DMA_BIT_MASK(31)`), not 32-bit / 4 GiB.
- Allocate DMA buffers from System RAM in **`0x40000000–0x7fffffff`**, never from default CMA at `0x3b800000`.
- `dma_alloc_coherent(..., len*8, ...)` — the r25 source allocated a hardcoded **4096** bytes then treated the buffer as `len*8`.
- `dma_free_coherent` must use the same bus address you allocated. r25 stores `phys >> 3` in the descriptor (bit 30:0) and then frees `phys` instead of `phys << 3`.

The community module in this repo carves a 16 MiB contiguous pool at `0x40000000` with `alloc_contig_range` and slices 32 KiB buffers from it. That is a workaround. The right fix is in your allocator + `libpcie`.

### 3. `rmmod` after RX oopses

**Severity: high (data-loss risk if NVMe shares the switch).**

`rmmod mymodule` after an RX attempt Oopses in `free_node`, then `Unable to change power state from D3cold to D0`. On a Pi 5 with NVMe on the same ASM1184e, **do not** reset the switch to recover — that is the OS disk. Reboot only.

**Ask:** make `remove`/`free_node` safe after streaming (refcount DMA bufs, do not free twice, do not put the function into D3cold while mappings exist). Document “reboot to reload”, not `rmmod`.

### 4. Userspace ABI and packaging

| Issue | Ask |
|---|---|
| Module name `mymodule`, driver `FPGA_PCIE`, node `/dev/FPGA` | Rename to `m2sdr` / `/dev/m2sdr` so it does not look like a tutorial driver and does not collide with LiteX `m2sdr.ko`. |
| PCI ID `10ee:7022` is stock **XDMA** | Use a unique ID, or document clearly that this is **not** XDMA. Official `xdma.ko` fails with `Failed to detect XDMA config BAR`. |
| Prebuilt `.ko` for a handful of Ubuntu kernels | Ship DKMS (`dkms.conf` is trivial). Prebuilts will not load on Pi OS / Debian 13 / Fedora. |
| `说明.txt` mojibake | Ship UTF-8 English **and** Chinese. Include: 4 KiB pages required; Pi 5 needs `pcie-32bit-dma-pi5`; never `cma=64M@1024M`; never `rmmod` after RX. |
| Kitchen-sink `apt_update.sh` | Split “driver+UHD” from “GNU Radio/Qt”. |
| USB B210 support dropped in the same prefix | Keep USB and PCIe as separate UHD transports, or install PCIe UHD to `/opt/m2sdr-uhd` by default. |
| `libpcie.a` closed source | Please open it, or at least ship a `.so` that uses `PAGE_SIZE` and a documented ioctl ABI so the kernel module and userspace can evolve independently. |

### 5. Application stack

Customers will try SDR++, Gqrx, GNU Radio. Those apps only work if they link **your** `libuhd.so`. Distro Gqrx/GNU Radio stay on Ettus 4.1 and will not see MyB210.

**Ask:** one paragraph in the handbook: “Rebuild SDR++ with `OPT_BUILD_USRP_SOURCE=ON` against this UHD; source name is USRP, `type=b200`. Distro UHD will not work.”

### 6. USB-style frames: default 3088 bytes is why 16 MS/s dies

**Severity: high (customers will think the card is 8 MS/s).**

Your UHD still talks to the FPGA like a USB B210. The default `recv_frame_size` is **3088 bytes** (`24*32*4+16`). Ettus B210s use ~8176. On this Pi 5 + EP-0180 (Gen2 x1, NVMe on the same switch):

| Args | Result |
|---|---|
| default ~3088 / 32 | lossless at 8 MS/s; **16 MS/s times out** (~7.7 M of 64 M samples) |
| `recv_frame_size=8176,num_recv_frames=32` | 4 s burst at 32 MS/s; 8 s times out |
| `recv_frame_size=8176,num_recv_frames=64` | **16 MS/s 10 s lossless**; 20 MS/s SDR++ OK; 24+ SDR++ dies; 32 MS/s 4 s burst then timeouts |
| `recv_frame_size=16360` | `ERROR_CODE_BAD_PACKET` / sequence errors |
| `num_recv_frames=128` | `uhd::assertion_error` |
| 50 MS/s or 61.44 MS/s, 8176 / 64 | AD9361 **does** set that master clock; then overruns / timeouts. 61.44 got 241 M of 246 M samples (not lossless). Teardown often `boost::lock_error` in `my_c2h_buf_cb`. |

PCIe Gen2 x1 average bandwidth is ~500 MB/s theoretical. 61.44 MS/s sc16 is 246 MB/s (~49%). This is **not** a wire-capacity problem. It is frame rate + `wait_for_ack` in closed `libpcie`.

**Ask:**

- Default `recv_frame_size` to **8176** (or document the args in the handbook). 3088 is a self-inflicted 16 MS/s cap.
- Do not advertise 61.44 MS/s on Pi 5 until `libpcie` can sustain it. **Run `benchmark_rate --duration 10`**, not 4 s. 4 s at 32–44 MS/s is a burst. Sustained community number is **20 MS/s** (SDR++), **16 MS/s** conservative.
- Open `libpcie` (or ship a `.so`) so C2H can use large DMA buffers instead of USB packets. That is the path to 50–61.44, not a faster HAT.
- FPGA x2: this bitstream advertises `max_link_width=2` but the Pi 5 FFC + ASM1184e uplink are x1. x2 will not help on this carrier. A desktop M.2 x2/x4 slot might.

Booting the OS from SD (NVMe idle) does not change the FPGA link width and did not fix 16 MS/s. The 3088-byte default did.

---

## Tests we recommend you run before the next zip

On **x86_64 4 KiB** (your usual) **and** on **Pi 5**:

1. `getconf PAGESIZE` printed in the log.
2. `uhd_find_devices` → MyB210.
3. `rx_samples_to_file --nsamps 20000 --rate 1e6` → non-zero file, `rx_exit=0`.
4. Same test at 8 MS/s with default args.
5. `benchmark_rate --args "type=b200,recv_frame_size=8176,num_recv_frames=64" --rx_rate 16e6` → 0 drops for **10 s**.
6. Same at 20e6, 24e6, 32e6, 61.44e6 with **duration 10**. Publish the first rate that drops.
7. HDMI still up after RX (Pi 5).
8. `dmesg` DMA addresses **not** in `0x3b000000–0x3fffffff` on Pi 5.
9. Do **not** `rmmod` as a test; if you do, it must not Oops.
10. Optional: SDR++ USRP source plays **60 s** at 20 MS/s with analog BW = sample rate. 32 MS/s must last 10 s, not 4 s.

If (3) fails on Pi 5 16 KiB, the mmap bug is still present. If (3) fails on Pi 5 4 KiB with addresses in `0x3bcxxxxx`, the CMA-hole bug is still present.

---

## Community tree

https://github.com/kingjamez/m2sdr-myb210

Contains your r25 source (`mymodule.c.vendor`), the DMA-pool module that streamed, udev/DKMS, vendor UHD zips, and Pi 5 notes. Happy to send a unified diff of `mymodule.c` vs r25 on request.
