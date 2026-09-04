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

**Not done in this module (needs vendor `libpcie.a`):** `mmap` lengths/offsets still assume 4 KiB. Host `PAGE_SIZE` must be 4096.

Do **not** use `cma=64M@1024M` as an alternative. It can place buffers at 1 GiB (RX works) and starves `vc4`, so HDMI dies.
