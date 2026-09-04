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

SDR++ does **not** ship a UHD source in every binary. You need a build with:

```bash
cmake .. -DOPT_BUILD_USRP_SOURCE=ON
```

linked against vendor `libuhd` 4.8 (`pkg-config --modversion uhd` → `4.8.0.0-0-unknown`).

In the UI:

1. Source: **USRP** (not “UHD”, not “Soapy”).
2. Device: `USRP b200 [<serial>]`.
3. Start at **1–8 MS/s** on a Pi 5 sharing PCIe x1 with NVMe.

A stock SDR++ without `usrp_source.so` cannot be “pointed at UHD” from settings. There is also no SoapyUHD module in a typical distro Soapy install, so Soapy is not a shortcut unless you build SoapyUHD against vendor 4.8 **and** SDR++ with `OPT_BUILD_SOAPY_SOURCE`.

## GNU Radio / Gqrx

`libgnuradio-uhd` and Gqrx are often still linked to `libuhd.so.4.1.0`. Check:

```bash
ldd "$(which gqrx)" | grep uhd
ldd /usr/local/lib/libgnuradio-uhd.so | grep uhd
```

If you see `libuhd.so.4.1.0`, rebuild GNU Radio’s UHD component (and Gqrx) against the vendor 4.8 prefix. Importing `from gnuradio import uhd` is not enough if that `.so` is the old one.

## Sample rate

The card is B210-like (AD9361, 2 RX / 2 TX). On a dedicated x86 x1/x2 slot you can try higher rates. On a Pi 5 + ASM1184e sharing the uplink with NVMe, start at 1 MS/s and climb only while the waterfall stays alive.
