# Patches

## `uhd-tx-dma-alignment.patch`

`scripts/install-uhd.sh` applies this patch to the unpacked vendor UHD 4.3–4.8 source before building. It pads the TX data OUT transfer to a multiple of 8 bytes before calling `libpcie`, preserving the CHDR length. This fixes truncated payloads on transfers whose length is a multiple of 4 bytes but not of 8, including one-sample EOB packets.

On an aarch64 Jetson with MyB210 and vendor UHD 4.8, a 16-packet TX sequence produced 12 EOB acknowledgements and 2 sequence errors before the fix. The rebuilt UHD produced 16 acknowledgements and zero sequence errors with the same sequence.

## `sdrpp-usrp-source-myb210.patch`

Against SDR++ 1.3 / current `master` `source_modules/usrp_source/src/main.cpp`.

Fixes verified on a HamGeek MyB210 (vendor UHD 4.8, Pi 5):

- Keep the `uhd::usrp::multi_usrp` alive after menu select (vendor `libpcie` C2H callbacks abort if it is destroyed).
- Set analog RX bandwidth to the sample rate (stock “Auto” programmed 200 kHz → ~50 dB bowl).
- Enable UHD DC-offset and IQ-balance auto-cal.
- Do not call `set_clock_source()` on every Play (GPSDO UART timeout).
- Ignore a bogus 0 Hz tune on first select.
- Inject `recv_frame_size=8176,num_recv_frames=64` on `make()` (HamGeek default 3088-byte frames die at 16 MS/s).
- Rate combo includes 1 / 2 / 4 / 8 / 16 / **20** MS/s (SDR++ sustained ceiling on this Pi).
- Drain overflows and restart the stream instead of freezing.
- Retry `set_rx_freq` up to 3 times in `tune()`: UHD throws `RX PLL NOT LOCKED` if the lock check runs early, and stepping the frequency quickly otherwise killed SDR++.

Fully quit SDR++ after installing `usrp_source.so`. See [docs/applications.md](../docs/applications.md) and [docs/sample-rate.md](../docs/sample-rate.md).
