# NOTICE

This repository contains a mix of licenses and a vendor binary blob.

## Linux kernel module (`pcie-driver/mymodule.c`)

`MODULE_LICENSE("GPL")`. Author: TQTT Liwei. Shipped by HamGeek as `b210_model_pcie_drv_r25`.

Small DMA-allocation fixes in `dma-fixes.patch` plus the Pi 5 DMA-pool work in `mymodule.c` are documented in `pcie-driver/COMMUNITY-CHANGES.md`.

## Ettus UHD

Most of `vendor/uhd-*.zip` is Ettus Research UHD, SPDX `GPL-3.0-or-later`. See `LICENSE.md` inside each zip.

Vendor modifications (also GPL where they are source) include:

- `host/lib/usrp/b200/CMakeLists.txt` — link `libpcie.a`
- `host/lib/usrp/b200/b200_impl.cpp` — PCIe discovery (`name: MyB210`)
- `host/lib/transport/libusb1_zero_copy.cpp` — calls into the static library
- `host/install_uhd.sh`

## Closed-source static libraries

`host/lib/usrp/b200/arm_libpcie.a` and `x64_libpcie.a` have no accompanying source. They are Linux ELF archives (`aarch64` and `x86_64`). HamGeek ships them as the UHD-mode support package for this card.

They are included so people who already have the hardware can rebuild. If you are the vendor and object to redistribution, open an issue and they can be replaced with a download-from-vendor step.

## Not LiteX-M2SDR

Enjoy Digital LiteX-M2SDR is a different product (`10ee:7024`, `m2sdr.ko`, SoapySDR). Do not mix those drivers with this tree.
