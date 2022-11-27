#!/usr/bin/env bash

APT_PKG_LIST="gdb libnuma-dev libcunit1 libcunit1-doc libcunit1-dev libaio-dev libncurses-dev libssl-dev uuid-dev python3-pip pkg-config meson libvulkan-dev libisal-dev build-essential git python3 gcc-10 g++-10 emacs zsh"

SUDO="${SUDO:-sudo}"
if [ "$EUID" -eq 0 ]; then
   export SUDO=""
fi

pip install pyelftools

"${SUDO}" apt update
"${SUDO}" apt install -y $APT_PKG_LIST
"${SUDO}" chsh -s /bin/zsh "${USER}"
