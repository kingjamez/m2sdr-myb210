#!/usr/bin/env bash
set -euo pipefail
sudo apt-get update
sudo apt-get install -y \
  build-essential dkms "linux-headers-$(uname -r)" \
  cmake pkg-config git unzip \
  libboost-all-dev libusb-1.0-0-dev libudev-dev \
  python3-dev python3-mako python3-numpy python3-requests python3-setuptools \
  python3-ruamel.yaml || {
    echo "If linux-headers-$(uname -r) failed, install the matching Raspberry Pi headers:" >&2
    echo "  sudo apt-get install linux-headers-rpi-2712 linux-headers-rpi-v8 build-essential dkms cmake ..." >&2
    exit 1
  }
