# Community changes vs vendor r25

`mymodule.c.vendor` is the file from `b210_model_pcie_drv_r25.zip`. `mymodule.c` is what this repo builds.

## First delta (`dma-fixes.patch`)

- `dma_free_coherent` / `pci_free_consistent` used `phys`; descriptors store `phys >> 3`, so free must use `phys << 3`.
- `dma_alloc_coherent` used a hardcoded 4096-byte buffer; it now uses `len*8`.

Discovery worked with the unpatched vendor source. Streaming did not.

## Pi 5 streaming (2026-09-04)

Verified: `rx_samples_to_file` `rx_exit=0`, 80000-byte IQ file, HDMI up, NVMe root.

| Change | Why |
|---|---|
| `DMA_BIT_MASK(31)` | Pi 5 PCIe inbound window is 2 GiB, not 4 GiB. 32-bit allocations can land above 2 GiB and never reach the FPGA. |
| Reject DMA in `0x3b000000–0x40000000` | That is default CMA / `reserved` on Pi 5. FPGA DMA there is invisible to UHD. |
| `alloc_contig_range` pool at `0x40000000`, 16 MiB (fallback 8 MiB) | Slices 32 KiB buffers with `dma=` in `0x40xxxxxx–0x77xxxxxx`, tagged `pool` in dmesg. Leaves default CMA for HDMI. |
| mmap via `dma_addr` PFNs | Userspace must see the same pages the FPGA writes. |
| Do not free pool slices individually | The whole pool is released on driver teardown. |

**Not done in this module (needs vendor `libpcie.a`):** `mmap` lengths/offsets still assume 4 KiB. Host `PAGE_SIZE` must be 4096. USB-style `recv_frame_size` (default 3088 bytes) is also in `libpcie`/UHD, not this module. Community workaround: pass `recv_frame_size=8176,num_recv_frames=64` — sustained through **20 MS/s** on this Pi (16 MS/s conservative). See [docs/sample-rate.md](../docs/sample-rate.md).

Do **not** use `cma=64M@1024M` as an alternative. It can place buffers at 1 GiB (RX works) and starves `vc4`, so HDMI dies.

## Completion-wait and teardown fixes (0.27, 2026-09-30)

Found while tracing the "dies above 20 MS/s" problem on an x86_64 host (Intel N100, IOMMU on). The main fix for that problem is in the closed `libpcie` (see `scripts/patch-libpcie.py` and [docs/sample-rate.md](../docs/sample-rate.md)); these are the driver-side parts.

| Change | Why |
|---|---|
| `az_read(NULL, 0)` returns 1 after a short timeout (`poll_ms`, default 2 ms) instead of 0 after 500 ms, and clears `rd_cond` once per wakeup | `libpcie`'s `do_cb` only re-reads the pending count (BAR0 `0x1c`) when this returns non-zero. A missed MSI left completions stranded. |
| `az_remove`: `pci_clear_master`, `free_irq`, `pci_disable_msi` **before** `free_node` | Stop the FPGA writing into buffers while they are freed. |
| Bounds checks on `idx` and `count` in `az_read` / `az_write`; check `copy_*_user` | Out-of-range offsets read or wrote kernel memory. |

Compile-tested on x86_64 (kernel 7.0). The same three changes ran for the x86_64 streaming tests in a driver built from r25 + `dma-fixes.patch`. Not yet run on a Pi 5.
