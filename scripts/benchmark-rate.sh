#!/usr/bin/env bash
# Lossless-rate RX check. Default args are what this Pi 5 actually sustains.
#   ./scripts/benchmark-rate.sh
#   RATE=44e6 DURATION=4 ./scripts/benchmark-rate.sh
# Never rmmod mymodule after this.
set -uo pipefail
export PATH="/usr/local/bin:/opt/m2sdr-uhd/bin:/usr/bin:/bin${PATH:+:$PATH}"

if [[ "$(getconf PAGESIZE)" != "4096" ]]; then
  echo "ERROR: PAGE_SIZE is not 4096. See docs/raspberry-pi-5.md." >&2
  exit 1
fi

ARGS="${ARGS:-type=b200,recv_frame_size=8176,num_recv_frames=64}"
RATE="${RATE:-40e6}"
DURATION="${DURATION:-4}"

EX=""
for c in \
  /usr/local/lib/uhd/examples/benchmark_rate \
  /opt/m2sdr-uhd/lib/uhd/examples/benchmark_rate \
  "$(command -v benchmark_rate 2>/dev/null || true)"
do
  if [[ -n "$c" && -x "$c" ]]; then EX=$c; break; fi
done
if [[ -z "$EX" ]]; then
  echo "ERROR: benchmark_rate not found. Build vendor UHD (scripts/install-uhd.sh)." >&2
  exit 1
fi

echo "===== $(date -Is) M2SDR benchmark_rate ====="
echo "args=$ARGS rate=$RATE duration=$DURATION"
echo "PAGESIZE=$(getconf PAGESIZE)  $(uname -r)"
set +e
sudo "$EX" --args "$ARGS" --rx_rate "$RATE" --duration "$DURATION"
echo "bench_exit=$?"
set -e
echo "===== done $(date -Is) ====="
echo "Expect 0 dropped / 0 overruns / 0 RX timeouts through 44 MS/s."
echo "See docs/sample-rate.md."
