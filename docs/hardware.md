# HamGeek M2SDR hardware (handbook translation)

Unofficial translation of `M2SDR上手手册` from the after-sales package.

## External connectors

1. **TXA** — channel A transmit
2. **RXA** — channel A receive; input power not above 0 dBm
3. **RXB** — channel B receive; input power not above 0 dBm
4. **TXB** — channel B transmit
5. **PPS** — PPS sync input
6. **M.2** — PCIe
7. **40 MHz clock out** — about 300 mV peak-to-peak
8. **External active clock in** — UHD default is 10 MHz
9. **GPS** — active GPS antenna (GPSDO)

## LEDs (LD1–LD10)

| LED | Color | Meaning |
|---|---|---|
| LD1 | red | TXB |
| LD2 | green | RXB |
| LD3 | green | RXA |
| LD4 | red | TXA |
| LD5 | red | undefined |
| LD6 | yellow | undefined |
| LD7 | purple | undefined |
| LD8 | green | undefined |
| LD9 | yellow | undefined |
| LD10 | blue | DONE (FPGA configured) |

## Environment (vendor steps)

1. Identify the device: `lspci | grep Xil` should show a 7021/7022/7024 Xilinx function.
2. Load the driver (build `mymodule.ko`, `sudo ./load_module.sh`). `ls /dev/FPGA` should succeed.
3. Install the vendor UHD tree. `sudo uhd_usrp_probe` should find the M2SDR.

PCI IDs seen in the driver:

| ID | Vendor comment |
|---|---|
| `10ee:7012` | default in source |
| `10ee:7021` | 1-lane model |
| `10ee:7022` | 2-lane model (reference card) |
| `10ee:7024` | 4-lane model |

There is also a second PCIe function on the reference board: `19aa:e004` “Signal processing controller”. UHD binds the Xilinx function, not this one.
