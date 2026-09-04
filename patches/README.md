# Patches

## `sdrpp-usrp-source-myb210.patch`

Against SDR++ 1.3 / current `master` `source_modules/usrp_source/src/main.cpp`.

Fixes verified on a HamGeek MyB210 (vendor UHD 4.8, Pi 5, 1 MS/s):

- Keep the `uhd::usrp::multi_usrp` alive after menu select (vendor `libpcie` C2H callbacks abort if it is destroyed).
- Set analog RX bandwidth to the sample rate (stock “Auto” programmed 200 kHz → ~50 dB bowl).
- Enable UHD DC-offset and IQ-balance auto-cal.
- Do not call `set_clock_source()` on every Play (GPSDO UART timeout).
- Ignore a bogus 0 Hz tune on first select.

See [docs/applications.md](../docs/applications.md).
