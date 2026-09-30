# Installing the high-sample-rate fix (2026-09)

If your M2SDR streams at low rates but dies within seconds at 20 MS/s or more (`ERROR_CODE_TIMEOUT`, then `AssertionError ... wait_for_ack`), this is the fix. It has three parts:

1. **`libpcie` patch** — the actual fix. The vendor library miscounts DMA completions once 64 or more are queued and deadlocks. One instruction is changed.
2. **Driver 0.27** — recovers from a missed interrupt instead of stalling, and unloads more safely.
3. **`recv_frame_size=12272`** — larger packets, so the host handles ~5× fewer interrupts per second.

With all three, an x86_64 host (Intel N100) streams 56 MS/s on one channel and 30.72 MS/s on two. The Raspberry Pi 5 has not been re-tested yet; please report your results.

Choose **A** if you already have the vendor UHD installed, **B** for a new install.

---

## A. Existing install (no UHD rebuild)

Close SDR++, GNU Radio and anything else using the radio first.

### 1. Get the updated repo

```bash
cd m2sdr-myb210          # your existing clone
git pull
```

New clone: `git clone https://github.com/kingjamez/m2sdr-myb210.git`

### 2. Patch the installed library

```bash
./scripts/apply-libpcie-fix.sh
```

It finds the vendor `libuhd` in `/usr/local/lib` or `/opt/m2sdr-uhd/lib`, saves the original as `libuhd.so.4.x.y.orig-<date>`, and installs the patched copy. Expected output:

```text
Vendor libuhd: /usr/local/lib/libuhd.so.4.8.0
/usr/local/lib/libuhd.so.4.8.0: patched (x86_64) at 0x...
Patched. Original saved as: /usr/local/lib/libuhd.so.4.8.0.orig-20260930-120000
```

Installed somewhere else? `LIBUHD=/path/to/libuhd.so.4.8.0 ./scripts/apply-libpcie-fix.sh`

Running it again prints `already patched` and changes nothing. If it prints `do_cb pattern not found`, your library is not the vendor 4.3–4.8 build; open an issue.

### 3. Update the driver

```bash
./scripts/install-driver.sh
sudo reboot
```

The script removes the old DKMS package (0.26), installs 0.27, and leaves the running module loaded. **Reboot to switch. Do not `rmmod mymodule`**: the old driver's unload path crashes the kernel.

After the reboot:

```bash
cat /sys/module/mymodule/parameters/poll_ms    # 2  -> driver 0.27 is running
```

### 4. Set the frame size

For every UHD program at once:

```bash
sudo mkdir -p /etc/uhd
printf '[type=b200]\nrecv_frame_size=12272\n' | sudo tee /etc/uhd/uhd.conf
```

Or per program: `--args "type=b200,recv_frame_size=12272"`. Device args override `uhd.conf`.

Do not use 16360 (sequence errors). `num_recv_frames` does nothing: the vendor code always uses 48.

### 5. Test

```bash
RATE=20e6 DURATION=30 ./scripts/benchmark-rate.sh
RATE=40e6 DURATION=30 ./scripts/benchmark-rate.sh
```

Pass = `Num dropped samples: 0`, `Num overruns detected: 0`, `Num timeouts (Rx): 0`. Before the fix, 20 MS/s stopped within seconds.

Single channel goes up to 56 MS/s (`RATE=56e6`). Two channels, up to 30.72 MS/s each (AD9361 limit):

```bash
sudo /usr/local/lib/uhd/examples/benchmark_rate --args "type=b200,recv_frame_size=12272" \
  --rx_subdev "A:A A:B" --channels 0,1 --rx_rate 30.72e6 --duration 30
```

On a small CPU, set the governor to `performance` for headroom:

```bash
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

### 6. SDR++

Re-apply [patches/sdrpp-usrp-source-myb210.patch](../patches/sdrpp-usrp-source-myb210.patch) to a clean SDR++ checkout, rebuild the USRP source, and fully quit SDR++ before replacing `usrp_source.so`. The update adds a tune retry: stepping the frequency quickly used to kill SDR++ with `RX PLL NOT LOCKED`.

The patch still offers up to 20 MS/s and forces 8176-byte frames (it overrides `uhd.conf`). SDR++'s own DSP is usually the limit above that on small CPUs.

---

## B. New install

Follow the [Quick start](../README.md#quick-start-any-linux-x86_64--aarch64-with-4-kib-pages). `install-uhd.sh` patches `libpcie` before building, and `install-driver.sh` installs 0.27. Then do steps **A4** (frame size) and **A5** (test).

Already built UHD from this repo before the fix? Either run step A2, or rebuild with `./scripts/install-uhd.sh`.

---

## Undo

- **Library:** `sudo mv /usr/local/lib/libuhd.so.4.8.0.orig-<date> /usr/local/lib/libuhd.so.4.8.0`
- **Frame size:** `sudo rm /etc/uhd/uhd.conf`
- **Driver:** `sudo dkms remove m2sdr/0.27 --all`, check out the previous commit, run `install-driver.sh`, then reboot.

## Troubleshooting

| Symptom | Check |
|---|---|
| Every UHD tool segfaults at startup | `/dev/FPGA` missing. `lspci -d 10ee:` empty means the card did not enumerate: reseat it, power-cycle, or try the other M.2 slot. |
| `already patched` but it still stalls | Is the program using this `libuhd`? `ldd $(which uhd_find_devices) \| grep libuhd` |
| `poll_ms` file missing after reboot | Old driver still loaded: `dkms status -m m2sdr`, then re-run `install-driver.sh` and reboot. |
| First open after quitting SDR++ fails with `wait_for_ack`; second works | Known; retry once. |
| Rare single overrun at 56 MS/s | Expected: only 48 RX frames (~2.6 ms buffer). |

Background and measurements: [sample-rate.md](sample-rate.md).
