#!/usr/bin/env bash
set -euo pipefail
sudo dnf install -y \
  gcc gcc-c++ make cmake dkms kernel-devel unzip \
  boost-devel libusb1-devel systemd-devel \
  python3-devel python3-mako python3-numpy python3-requests python3-setuptools
