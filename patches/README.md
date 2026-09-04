# Patches

## `sdrpp-usrp-source-myb210.patch`

Against SDR++ 1.3 / current `master` `source_modules/usrp_source/src/main.cpp`.

Fixes verified on a HamGeek MyB210 (vendor UHD 4.8, Pi 5):

- Keep the `uhd::usrp::multi_usrp` alive after menu select (vendor `libpcie` C2H callbacks abort if it is destroyed).
- Set analog RX bandwidth to the sample rate (stock “Auto” programmed 200 kHz → ~50 dB bowl).
- Enable UHD DC-offset and IQ-balance auto-cal.
- Do not call `set_clock_source()` on every Play (GPSDO UART timeout).
- Ignore a bogus 0 Hz tune on first select.
- Inject `recv_frame_size=8176,num_recv_frames=64` on `make()` (HamGeek default 3088-byte frames die at 16 MS/s).
- Rate combo includes 1 / 2 / 4 / 8 / 16 / 32 / **40 / 44** MS/s (community lossless ceiling on this Pi).

Fully quit SDR++ after installing `usrp_source.so`. See [docs/applications.md](../docs/applications.md) and [docs/sample-rate.md](../docs/sample-rate.md).
