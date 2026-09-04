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
| `DMA_BIT_MASK(31)` | Pi 5 PCIe inbound window is 2 GiB, not 4 GiB. |
| Reject DMA in `0x3b000000–0x40000000` | Default CMA / `reserved` on Pi 5. FPGA DMA there is invisible to UHD. |
| `alloc_contig_range` pool at `0x40000000`, 16 MiB (fallback 8 MiB) | Leaves default CMA for HDMI. |
| mmap via `dma_addr` PFNs | Userspace must see the same pages the FPGA writes. |

**Not done in this module (needs vendor `libpcie.a`):** `mmap` lengths/offsets still assume 4 KiB. Host `PAGE_SIZE` must be 4096. USB-style `recv_frame_size` (default 3088 bytes) is also in `libpcie`/UHD. Community workaround: `recv_frame_size=8176,num_recv_frames=64` — sustained through **20 MS/s** on this Pi (16 MS/s conservative). See [docs/sample-rate.md](../docs/sample-rate.md).

Do **not** use `cma=64M@1024M`. RX can work; HDMI dies.
