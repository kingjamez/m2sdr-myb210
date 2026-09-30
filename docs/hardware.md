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

## GPSDO and internal oscillator (measured 2026-09-30)

Reference card on an x86_64 host, active GPS antenna on the u.fl GPS input.

| Check | Result |
|---|---|
| GPS fix | < 1 min after boot (5 satellites); `gps_locked=true`, NMEA status `A`, `gps_time` correct |
| `gps_servo` | Not implemented by this GPSDO (`No SERVO message found`) |
| `clock_source=gpsdo` | `ref_locked=true` immediately; oscillator steered from ±16 ppm to within ±1 tick per 10 s (≤ 6 ppb, the measurement floor) in ~20 s, held for 5 min |
| `time_source=gpsdo` + `set_time_next_pps(gps_time + 1)` | Device time matched GPS seconds on every PPS |
| **Internal** oscillator vs GPS PPS | **−4.0 ppm** (runs slow), steady within 6 ppb over 5 min; long-term / temperature drift not measured |

UHD starts on the internal oscillator. Select `gpsdo` for clock and time in your program (or add `clock_source=gpsdo,time_source=gpsdo` to the device args) when you need it. With the internal reference, signals show about 4 ppm high: ~400 Hz at 100 MHz, ~4 kHz at 1 GHz.

## Host slot notes

On a UGREEN DXP2800 (Intel N100) only one of the two M.2 slots trains the card; in the other the BIOS hides the root port and the card never enumerates (no `10ee:` device, no `/dev/FPGA`, and every UHD tool then segfaults in `libpcie`). If `lspci -d 10ee:` is empty after reseating, try the other slot before debugging software.
