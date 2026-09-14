#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 5 ]; then
  echo "Usage: build-opus-macos.sh VERSION SHA256 DEPLOYMENT_TARGET ARCH PREFIX" >&2
  exit 2
fi

version="$1"
sha256="$2"
deployment_target="$3"
arch="$4"
prefix="$5"

case "$arch" in
  arm64|x86_64) ;;
  *)
    echo "Unsupported macOS architecture: $arch" >&2
    exit 2
    ;;
esac

archive="opus-${version}.tar.gz"
curl -L --fail --retry 5 --retry-delay 10 \
  -o "$archive" \
  "https://github.com/xiph/opus/releases/download/v${version}/${archive}"
printf '%s  %s\n' "$sha256" "$archive" | shasum -a 256 -c -
tar -xzf "$archive"
cd "opus-${version}"

export MACOSX_DEPLOYMENT_TARGET="$deployment_target"
./configure \
  --prefix="$prefix" \
  --enable-shared --disable-static \
  CFLAGS="-arch ${arch} -mmacosx-version-min=${deployment_target}" \
  LDFLAGS="-arch ${arch} -mmacosx-version-min=${deployment_target}"
make -j"$(sysctl -n hw.ncpu)"
make install

test -f "$prefix/lib/libopus.0.dylib"
