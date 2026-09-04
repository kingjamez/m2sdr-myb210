# Applications (UHD, GNU Radio, Gqrx, SDR++)

The radio is a UHD **B200-family** device (`type=b200`, name `MyB210`). Any program that talks to a B210 through **this** `libuhd.so` can use it. Programs linked against distro UHD or `libuhd.so.4.1.0` cannot.

## UHD CLI (vendor 4.8 in `/usr/local` or your PREFIX)

```bash
uhd_config_info --version          # 4.8.0.0-0-unknown
uhd_find_devices
uhd_usrp_probe --args "type=b200"
/usr/local/lib/uhd/examples/rx_samples_to_file \
  --args "type=b200" --nsamps 20000 --rate 1e6 --freq 100e6 --gain 40 \
  --file /tmp/m2sdr_rx.dat
/usr/local/lib/uhd/examples/rx_ascii_art_dft \
  --args "type=b200" --rate 1e6 --freq 100e6 --gain 40
```

`./scripts/verify-rx.sh` is the same RX test with extra PCIe/dmesg checks.

TX examples (`tx_waveforms`, `tx_samples_from_file`) ship with UHD. They have not been the focus of the reference bring-up.

## SDR++

SDR++ does **not** ship a UHD source in every binary. You need a build with `-DOPT_BUILD_USRP_SOURCE=ON` linked against vendor `libuhd` 4.8 (`pkg-config --modversion uhd` → `4.8.0.0-0-unknown`).

Stock `usrp_source` is not safe on this card. Apply [patches/sdrpp-usrp-source-myb210.patch](../patches/sdrpp-usrp-source-myb210.patch) (SDR++ 1.3 / current master) before building, or copy the patched `usrp_source.so` over `/usr/lib/sdrpp/plugins/usrp_source.so` and **fully quit and reopen** SDR++.

```bash
git clone https://github.com/AlexandreRouma/SDRPlusPlus.git
cd SDRPlusPlus
patch -p1 < /path/to/m2sdr-myb210/patches/sdrpp-usrp-source-myb210.patch
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DOPT_BUILD_USRP_SOURCE=ON
ninja usrp_source   # or a full ninja
sudo cp source_modules/usrp_source/usrp_source.so /usr/lib/sdrpp/plugins/
```

### UI

1. Source: **USRP** (not “UHD”, not “Soapy”).
2. Device: `USRP b200 [<serial>]`.
3. Sample rate **1 MS/s** on a Pi 5 that shares PCIe x1 with NVMe. Try 2 MS/s only after 1 MS/s looks clean. 4 MS/s plus a 64k FFT has bus-errored this host.
4. FFT size **8192** (65536 is too heavy on the Pi 5 GPU).
5. Hit Play. Fully quit the app after replacing the plugin; Stop is not enough.

### Front-end setup (verified 2026-09-04)

Stock SDR++ “Auto” bandwidth plus a local `multi_usrp` that is destroyed on menu-select produced two classes of failure:

| What you see | Cause | What the patch does |
|---|---|---|
| Instant crash / abort when USRP is selected | `select()` `make()`s the device, then the local `shared_ptr` dies while vendor `libpcie` C2H callbacks (`my_c2h_buf_cb` → `boost::lock_error`) are still running | Keep the `usrp` object alive until process exit; do not `dev.reset()` on Stop |
| Noise floor ~50 dB higher at center than at the edges; extra spurs | Analog BB LPF programmed to the AD9361 **200 kHz minimum** (uninitialized “Auto” bandwidth index) while sampling at 1 MS/s | Set analog RX bandwidth **equal to the sample rate** |
| DC spike / IQ image spurs | DC offset and IQ balance auto-cal never enabled | `set_rx_dc_offset(true)` and `set_rx_iq_balance(true)` after tune |
| GPSDO UART / `fifo ctrl timed out` | `set_clock_source()` on every Play re-inits the GPSTCXO UART | Leave the clock source that `make()` chose (internal) unless the user picks another |

After the analog-BW fix, real signals sit where they should and the waterfall is much flatter. Residual hash from PCIe sharing the ASM1184e with NVMe can still be there; it should no longer look like a 50 dB analog bowl.

Do **not** `rmmod mymodule` if SDR++ dies. Reboot.

A stock SDR++ without `usrp_source.so` cannot be “pointed at UHD” from settings. There is also no SoapyUHD module in a typical distro Soapy install.

## GNU Radio / Gqrx

`libgnuradio-uhd` and Gqrx are often still linked to `libuhd.so.4.1.0`. Check:

```bash
ldd "$(which gqrx)" | grep uhd
ldd /usr/local/lib/libgnuradio-uhd.so | grep uhd
```

If you see `libuhd.so.4.1.0`, rebuild GNU Radio’s UHD component (and Gqrx) against the vendor 4.8 prefix. Importing `from gnuradio import uhd` is not enough if that `.so` is the old one.

## Sample rate

The card is B210-like (AD9361, 2 RX / 2 TX). On a dedicated x86 x1/x2 slot you can try higher rates. On a Pi 5 + ASM1184e sharing the uplink with NVMe, start at 1 MS/s and climb only while the waterfall stays alive.
