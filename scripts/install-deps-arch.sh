#!/usr/bin/env bash
set -euo pipefail
sudo pacman -S --needed \
  base-devel cmake dkms linux-headers unzip \
  boost libusb python python-mako python-numpy python-requests python-setuptools
