# mymodule (FPGA_PCIE)

Out-of-tree Linux driver for the HamGeek M2SDR.

- PCI IDs: `10ee:7012`, `10ee:7021` (x1), `10ee:7022` (x2), `10ee:7024` (x4)
- Character device: `/dev/FPGA`
- DMA mask: 32-bit
- `LO_KER` is left undefined (required on Ubuntu 22+ / modern kernels)

Build against the **running** kernel:

```bash
make
sudo ./load_module.sh
ls /dev/FPGA
```

Or use `../scripts/install-driver.sh` for DKMS.

`mymodule.c.vendor` is the unmodified file from `b210_model_pcie_drv_r25.zip`. `dma-fixes.patch` is the delta applied in this tree.
