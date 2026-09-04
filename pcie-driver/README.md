# mymodule (FPGA_PCIE)

Out-of-tree Linux driver for the HamGeek M2SDR. DKMS package version **0.26** (community).

- PCI IDs: `10ee:7012`, `10ee:7021` (x1), `10ee:7022` (x2), `10ee:7024` (x4)
- Character device: `/dev/FPGA`
- DMA mask: **31-bit** (2 GiB), matching the Pi 5 PCIe inbound window
- `LO_KER` is left undefined (required on Ubuntu 22+ / modern kernels)
- Community DMA pool: contiguous pages in `0x40000000–0x78000000` so FPGA DMA does not land in the Pi 5 64 MiB CMA hole (that hole is HDMI’s). See [COMMUNITY-CHANGES.md](COMMUNITY-CHANGES.md).

`mymodule.c.vendor` is unmodified `b210_model_pcie_drv_r25`. `dma-fixes.patch` is the first small delta (free address `<< 3`, alloc size `len*8`). `mymodule.c` is the driver that **streamed** on the reference Pi 5.

Build against the **running** kernel:

```bash
make
sudo ./load_module.sh
ls /dev/FPGA
```

Prefer `../scripts/install-driver.sh` (DKMS). If `mymodule` is already loaded, that script will **not** `rmmod` it — reboot instead. `rmmod` after RX Oopses.
