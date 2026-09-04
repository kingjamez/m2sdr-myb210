# Applications (UHD, GNU Radio, Gqrx, SDR++)

The radio is a UHD **B200-family** device (`type=b200`, name `MyB210`). Any program that talks to a B210 through **this** `libuhd.so` can use it. Programs linked against distro UHD or `libuhd.so.4.1.0` cannot.

## UHD CLI (vendor 4.8 in `/usr/local` or your PREFIX)

```bash
uhd_config_info --version          # 4.8.0.0-0-unknown
uhd_find_devices
uhd_usrp_probe --args "type=b200"
/usr/local/lib/uhd/examples/rx_samples_to_file \
  --args "type=b200,recv_frame_size=8176,num_recv_frames=64" \
  --nsamps 20000 --rate 1e6 --freq 100e6 --gain 40 \
  --file /tmp/m2sdr_rx.dat
/usr/local/lib/uhd/examples/benchmark_rate \
  --args "type=b200,recv_frame_size=8176,num_recv_frames=64" \
  --rx_rate 20e6 --duration 10
```

`./scripts/verify-rx.sh` is the same RX test with extra PCIe/dmesg checks.

## SDR++

Rebuild with `-DOPT_BUILD_USRP_SOURCE=ON` against vendor UHD 4.8 and apply [patches/sdrpp-usrp-source-myb210.patch](../patches/sdrpp-usrp-source-myb210.patch). Fully quit and reopen after installing `usrp_source.so`.

1. Source: **USRP**. Device: `USRP b200 [<serial>]`.
2. Sample rate **16 MHz** or **20 MHz**. 24+ dies in seconds.
3. FFT size **8192**.
4. Analog BW is Auto (= sample rate). Do not poke the GPSDO clock on Play.

## Sample rate

| Rate | Result |
|---|---|
| **16 / 20 MS/s** | sustained (SDR++ and 8–10 s UHD) |
| 24 MS/s | 8 s UHD burst OK; **SDR++ dies** |
| 32–44 MS/s | 4 s UHD burst only; then timeouts |
| 48 / 50 / 61.44 MS/s | clocks, then overruns |

Full table: **[sample-rate.md](sample-rate.md)**.
