# Original HamGeek after-sales files

English names used in this repo:

| Original (Chinese) | Here |
|---|---|
| `M2SDR售后文件/` | this repository |
| `UHD模式/` | `vendor/` |
| `b210_model_pcie_drv_r25.zip` | PCIe driver source |
| `uhd-4.3.0.0.zip` … `uhd-4.8.0.0.zip` | vendor-patched UHD |
| `说明.txt` | `vendor-uhd-instructions.zh.txt` |
| `linux安装库参考脚本/apt_update.sh` | `scripts/vendor-apt_update.sh` |

Last vendor update stamp: `last-updated-2026-05-27.txt`.

Translated vendor UHD notes:

1. This is UHD modified for M2SDR. After you install it, USB B210 is not supported.
2. Unpack one version; example is 4.8.0.0.
3. Go to `uhd-4.8.0.0/host` and use `install_uhd.sh`.
4. The script is meant for x86_64 and ARM Linux (Raspberry Pi, Orange Pi, NanoPC).
5. With the kernel driver loaded, `sudo uhd_usrp_probe` should find the card.

The large `uhd-*.zip` files (41–44 MB each) and `libpcie.a` binaries are prepared locally at `~/src/m2sdr-myb210/vendor` and `uhd-overlay/`. GitHub's API path used to create this repo does not accept those binaries in one go; copy them from that directory (or from `~/M2SDR/UHD-mode/`) into a clone and `git add` / `git push`, or attach them as a GitHub Release.
