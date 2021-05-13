#!/bin/sh

sudo apt-get update
sudo sudo DEBIAN_FRONTEND=noninteractive apt-get install -y cmake pkg-config rpm file \
  libjack-dev
