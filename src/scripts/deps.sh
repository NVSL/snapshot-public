#!/usr/bin/env bash

APT_PKG_LIST="libboost-exception-dev libpmemobj-dev libpmemobj-cpp-dev flex bison libelf-dev dwarves zstd cmake gdb libnuma-dev libcunit1 libcunit1-doc libcunit1-dev libaio-dev libncurses-dev libssl-dev uuid-dev python3-pip pkg-config meson libvulkan-dev libisal-dev build-essential git python3 gcc-10 g++-10 emacs zsh"

SUDO="${SUDO:-sudo}"
if [ "$EUID" -eq 0 ]; then
   export SUDO=""
fi

# Install DEPS
"${SUDO}" apt update
"${SUDO}" apt install -y $APT_PKG_LIST
pip install pyelftools


# Setup Linux
cd ~
wget 'https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.0.10.tar.xz'
tar xf linux-6.0.10.tar.xz
cd linux-6.0.10
cp ~/cxlbuf/src/scripts/.config ./

make -j$(nproc --all)
sudo make -j$(nproc --all)
sudo make modules_install -j$(nproc --all)
sudo make install -j$(nproc --all)

