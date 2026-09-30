#!/usr/bin/env bash
# Apply the libpcie completion-deadlock fix to an ALREADY INSTALLED vendor
# libuhd (no UHD rebuild needed). Keeps a dated backup next to the library.
#
#   ./scripts/apply-libpcie-fix.sh                 # finds libuhd automatically
#   LIBUHD=/opt/m2sdr-uhd/lib/libuhd.so.4.8.0 ./scripts/apply-libpcie-fix.sh
#
# Undo: sudo mv <backup> <library>   (the backup path is printed below)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PATCHER="$ROOT/scripts/patch-libpcie.py"

if [[ -z "${LIBUHD:-}" ]]; then
  for c in /usr/local/lib/libuhd.so.4.* /opt/m2sdr-uhd/lib/libuhd.so.4.* \
           /usr/local/lib/*/libuhd.so.4.* /opt/m2sdr-uhd/lib/*/libuhd.so.4.*; do
    [[ -f "$c" && ! -L "$c" && "$c" =~ libuhd\.so\.[0-9.]+$ ]] || continue
    # Only the vendor build contains libpcie.
    if grep -qa 'dma buff MMAP failed' "$c"; then LIBUHD=$c; break; fi
  done
fi
if [[ -z "${LIBUHD:-}" || ! -f "$LIBUHD" ]]; then
  echo "Could not find the vendor libuhd (the one built from vendor/uhd-*.zip)." >&2
  echo "Set LIBUHD=/path/to/libuhd.so.4.x.y and run again." >&2
  exit 1
fi
LIBUHD="$(readlink -f "$LIBUHD")"
echo "Vendor libuhd: $LIBUHD"

if pgrep -f 'sdrpp|gnuradio|gqrx|uhd_|benchmark_rate|rx_samples' >/dev/null; then
  echo "NOTE: programs that use the radio are running. They keep the old library"
  echo "      until restarted; restart them after this finishes."
fi

TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT
cp "$LIBUHD" "$TMP"
OUT="$(python3 "$PATCHER" "$TMP")" || { echo "$OUT" >&2; exit 1; }
echo "$OUT" | sed "s|$TMP|$LIBUHD|"
if grep -q 'already patched' <<<"$OUT"; then
  echo "Nothing to do."
  exit 0
fi

BACKUP="$LIBUHD.orig-$(date +%Y%m%d-%H%M%S)"
sudo cp -p "$LIBUHD" "$BACKUP"
# Install as a new file and rename over the old one, so running programs that
# still have the old library mapped are not disturbed.
sudo install -m 0644 "$TMP" "$LIBUHD.new"
sudo mv -f "$LIBUHD.new" "$LIBUHD"
echo "Patched. Original saved as: $BACKUP"
echo "Next: set recv_frame_size=12272 (docs/upgrading.md) and run ./scripts/benchmark-rate.sh"
