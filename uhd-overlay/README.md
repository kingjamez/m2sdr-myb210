# UHD overlay (from vendor `uhd-4.8.0.0`)

These files are the HamGeek glue on top of Ettus UHD 4.8. Prefer unpacking `vendor/uhd-4.8.0.0.zip` and building that tree. This directory exists so you can see exactly what is not stock Ettus.

| File | Why it is here |
|---|---|
| `host/lib/usrp/b200/CMakeLists.txt` | Links `x64_libpcie.a` or `arm_libpcie.a` plus pthread |
| `host/lib/usrp/b200/b200_impl.cpp` | Replaces USB discovery with `func_xxXx` / serial from `libpcie`; device name **MyB210** |
| `host/lib/usrp/b200/arm_libpcie.a` | Closed-source Linux aarch64 PCIe transport |
| `host/lib/usrp/b200/x64_libpcie.a` | Closed-source Linux x86_64 PCIe transport |
| `host/lib/transport/libusb1_zero_copy.cpp` | Calls `func_xxxX` / `func_xxxx` in that library |
| `host/install_uhd.sh` | Vendor build/install script |

There is no macOS object file.
