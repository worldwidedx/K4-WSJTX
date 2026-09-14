#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 5 ]; then
  echo "Usage: build-openssl-macos.sh VERSION SHA256 DEPLOYMENT_TARGET ARCH PREFIX" >&2
  exit 2
fi

version="$1"
sha256="$2"
deployment_target="$3"
arch="$4"
prefix="$5"

case "$arch" in
  arm64) configure_target="darwin64-arm64-cc" ;;
  x86_64) configure_target="darwin64-x86_64-cc" ;;
  *)
    echo "Unsupported macOS architecture: $arch" >&2
    exit 2
    ;;
esac

archive="openssl-${version}.tar.gz"
curl -L --fail --retry 5 --retry-delay 10 \
  -o "$archive" \
  "https://github.com/openssl/openssl/releases/download/openssl-${version}/${archive}"
printf '%s  %s\n' "$sha256" "$archive" | shasum -a 256 -c -
tar -xzf "$archive"
cd "openssl-${version}"

export MACOSX_DEPLOYMENT_TARGET="$deployment_target"
export CFLAGS="-arch ${arch} -mmacosx-version-min=${deployment_target}"
export LDFLAGS="-arch ${arch} -mmacosx-version-min=${deployment_target}"

./Configure "$configure_target" \
  shared no-tests no-docs \
  --prefix="$prefix" \
  --libdir=lib \
  --openssldir="$prefix/ssl"
make -j"$(sysctl -n hw.ncpu)"
make install_sw

test -f "$prefix/lib/libssl.3.dylib"
test -f "$prefix/lib/libcrypto.3.dylib"
