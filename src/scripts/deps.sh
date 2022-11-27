#!/usr/bin/env bash

APT_PKG_LIST="libvulkan-dev libisal-dev build-essential git python3 gcc-10 g++-10"

SUDO="${SUDO:-sudo}"
if [ "$EUID" -eq 0 ]; then
   export SUDO=""
fi

${SUDO} apt install -y $APT_PKG_LIST
