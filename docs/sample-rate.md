# Sample rate on Raspberry Pi 5

How to get the highest **usable** RX rate on the reference host (Pi 5 + 52Pi EP-0180 + NVMe root + community `mymodule` + vendor UHD 4.8).

The AD9361 will clock any of 32 / 40 / 44 / 48 / 50 / 56 / 61.44 MHz. That is not the limit. The limit is vendor `libpcie` talking to the FPGA like a USB B210.

**Sustained ceiling (SDR++ and long UHD runs): 20 MS/s.**  
**16 MS/s is the rock-solid daily rate.** 24+ dies in a few seconds. 32–44 MS/s can look lossless for a **4 s** `benchmark_rate` and then fall over — those 4 s numbers are bursts, not a ceiling. 50+ needs HamGeek.

---

## Device args (the one change that mattered)

HamGeek’s default USB frame is **3088 bytes** (`24*32*4+16`). Ettus B210s use ~**8176**. On this transport, 3088 bytes dies at 16 MS/s. 8176/64 is what makes **16 and 20 MS/s** last. 4-second runs at 32–44 MS/s with the same args are not sustainable.

Use this on **every** UHD tool, including SDR++:

```text
type=b200,recv_frame_size=8176,num_recv_frames=64
```

```bash
/usr/local/lib/uhd/examples/benchmark_rate \
  --args "type=b200,recv_frame_size=8176,num_recv_frames=64" \
  --rx_rate 20e6 --duration 10
```

Or `./scripts/benchmark-rate.sh`. Use `--duration 10` or more; 4 s hides the 32 MS/s collapse. Do **not** use `recv_frame_size=16360`.

**Longer runs (what you can actually use):**

| Rate | Tool | Result |
|---|---|---|
| 16 MS/s | UHD 10 s / SDR++ | lossless / rock-solid |
| 20 MS/s | UHD 8 s / **SDR++** | lossless / **usable ceiling** |
| 24 MS/s | UHD 8 s / SDR++ | burst OK / **dies** |
| 32–44 MS/s | UHD 4 s / SDR++ | burst only / dies ~4 s |
| 50–61.44 MS/s | UHD | clocks, then overruns |

Average PCIe Gen2 x1 is enough for 61.44 MS/s sc16. The stall is USB-style frames and `wait_for_ack` in closed `libpcie`.

SDR++ combo is 1 / 2 / 4 / 8 / 16 / **20** MS/s. Use **16 or 20 MHz**, FFT 8192. Fully quit after installing the plugin.

HamGeek owns everything above this: open `libpcie`, default 8176-byte frames, PAGE_SIZE mmap, sustained 32–61.44. See [VENDOR-FEEDBACK.md](../VENDOR-FEEDBACK.md).
