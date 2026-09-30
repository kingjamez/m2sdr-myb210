#!/usr/bin/env bash
# Unpack vendor UHD (default 4.8.0.0) and install to PREFIX (default /usr/local).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${UHD_VERSION:-4.8.0.0}"
PREFIX="${PREFIX:-/usr/local}"
ZIP="$ROOT/vendor/uhd-${VERSION}.zip"
WORK="${WORK:-/tmp/m2sdr-uhd-${VERSION}}"

if [[ ! -f "$ZIP" ]]; then
  echo "Missing $ZIP" >&2
  echo "This repo should contain vendor/uhd-${VERSION}.zip from the HamGeek after-sales package." >&2
  exit 1
fi

echo "Using $ZIP"
echo "Install prefix: $PREFIX"
echo "Build dir: $WORK"
echo
echo "NOTE: this UHD build is for the M2SDR PCIe card (MyB210)."
echo "The vendor says USB B210 support is dropped once this is installed into the same prefix."
echo

rm -rf "$WORK"
mkdir -p "$WORK"
unzip -q "$ZIP" -d "$WORK"
# zip contains a top-level uhd-x.y.z.w directory
HOST="$(find "$WORK" -maxdepth 2 -type d -name host | head -n1)"
if [[ -z "$HOST" || ! -f "$HOST/CMakeLists.txt" ]]; then
  echo "Could not find host/CMakeLists.txt inside $ZIP" >&2
  exit 1
fi

# Fix the vendor libpcie completion-count bug (do_cb masks the pending count
# with 0x3f; a backlog of 64+ deadlocks RX and the control path). The vendor
# zip is left untouched; only the unpacked copy is patched.
python3 "$ROOT/scripts/patch-libpcie.py" "$HOST"/lib/usrp/b200/*_libpcie.a

mkdir -p "$HOST/build"
cd "$HOST/build"
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"
make -j"$(nproc)"
sudo make install
sudo ldconfig

echo
echo "Installed. Check with:"
echo "  $PREFIX/bin/uhd_config_info --version"
echo "  sudo $PREFIX/bin/uhd_find_devices"
echo "  sudo $PREFIX/bin/uhd_usrp_probe"
