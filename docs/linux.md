# Generic Linux (x86_64 and aarch64)

Vendor after-sales notes say the UHD package is for **Linux x86_64 and Linux aarch64** (Raspberry Pi, Orange Pi, NanoPC, …). Ubuntu 18.04/20.04/22.04/24.04/25.04 are the platforms they ship prebuilt `.ko` files for. This repo always builds from source against the running kernel.

## Checklist

1. **4 KiB pages.** `getconf PAGESIZE` must print `4096`. If you are on a 16 KiB distro kernel (some Pi 5, some aarch64), streaming will fail even if discovery works.
2. **M.2 M-key slot that is real PCIe**, not an NVMe-only connector. `lspci` must show `10ee:7021` / `7022` / `7024`.
3. **Kernel headers** for `uname -r`.
4. **This repo’s driver + vendor UHD 4.8**, not distro UHD.

```bash
./scripts/install-deps-debian.sh    # or fedora / arch
./scripts/install-driver.sh
./scripts/install-uhd.sh            # PREFIX=/opt/m2sdr-uhd to keep USB B210s
./scripts/verify-rx.sh
```

## x86_64 notes

Most desktop/server kernels already use 4 KiB pages, which is why the vendor primarily tested there. You still need:

- the kernel module (prebuilt Ubuntu `.ko` files from HamGeek will not load on a foreign kernel; build from this tree)
- vendor UHD, not Ettus main
- `LO_KER` left **undefined** (already true in this tree) on Ubuntu 22+

The community `mymodule.c` uses a 31-bit DMA mask (2 GiB) and prefers a contiguous pool at 1 GiB. That is Pi 5 hardening; on a typical x86 box the pool allocator falls back to ordinary `dma_alloc_*` if `alloc_contig_range` cannot claim that range. If verify-rx fails, `dmesg | grep m2sdr` is the first place to look.

## aarch64 SBCs other than Pi 5

Same userspace path. Check:

- `getconf PAGESIZE` is 4096
- the SoC’s PCIe inbound window covers the DMA addresses the driver prints (`dmesg`: `alloc idx=0 … dma=`)
- ASPM: if the link flaps, try `pcie_aspm=off pci=noaer` on the cmdline

Pi 5-specific overlays (`pcie-32bit-dma-pi5`, `kernel8.img`) do not apply. See [raspberry-pi-5.md](raspberry-pi-5.md) only if you are actually on a Pi 5.

## Prefix isolation

Installing vendor UHD into `/usr/local` **replaces** USB B210 support in that prefix. Keep a separate Ettus tree if you still use USB B2xx:

```bash
PREFIX=/opt/m2sdr-uhd ./scripts/install-uhd.sh
export PATH=/opt/m2sdr-uhd/bin:$PATH
export LD_LIBRARY_PATH=/opt/m2sdr-uhd/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
```

GNU Radio / Gqrx / SDR++ must be built against the same `libuhd.so`.

For high sample rates, pass `recv_frame_size=8176,num_recv_frames=64`. HamGeek’s 3088-byte default dies at 16 MS/s even on a dedicated slot. Pi 5 sustained ceiling is **20 MS/s** (16 MS/s conservative): [sample-rate.md](sample-rate.md).
