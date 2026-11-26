#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: build-hamlib-windows.sh BRANCH PREFIX" >&2
  exit 2
fi

branch="$1"
prefix="$2"

git clone --depth 1 --branch "$branch" \
  https://github.com/Hamlib/Hamlib.git hamlib-src
cd hamlib-src
# Hamlib 4.7.2 uses assert() in the Windows timing code without its header.
if [ "$branch" = "4.7.2" ]; then
  sed -i '1i#include <assert.h>' lib/precise_time.c
fi
./bootstrap
./configure \
  --prefix="$prefix" \
  --enable-shared --disable-static \
  --without-cxx-binding \
  --without-readline \
  CFLAGS="-DNDEBUG -g -O2 -fdata-sections -ffunction-sections" \
  LDFLAGS="-Wl,--gc-sections"
make -j"$(nproc)"
make install-strip
