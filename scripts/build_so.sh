#!/usr/bin/env bash
# Build libentpass.so for arm64-v8a and (optionally) armeabi-v7a
# Requires Android NDK r23+
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$SCRIPT_DIR/../src/libentpass.cpp"
OUT_DIR="$SCRIPT_DIR/../out"
mkdir -p "$OUT_DIR/arm64-v8a" "$OUT_DIR/armeabi-v7a"

# Locate NDK
if [[ -z "${ANDROID_NDK_HOME:-}" && -z "${NDK_ROOT:-}" ]]; then
  for d in \
    "$HOME/Android/Sdk/ndk/"* \
    /opt/android-ndk* \
    /usr/local/android-ndk* \
    "$HOME/ndk" \
    /opt/ndk; do
    if [[ -d "$d" ]]; then
      export ANDROID_NDK_HOME="$d"
      break
    fi
  done
fi

NDK="${ANDROID_NDK_HOME:-${NDK_ROOT:-}}"
if [[ -z "$NDK" || ! -d "$NDK" ]]; then
  echo "ERROR: set ANDROID_NDK_HOME to your NDK path"
  echo "  export ANDROID_NDK_HOME=/path/to/ndk"
  exit 1
fi

API=21
HOST_TAG=linux-x86_64
if [[ "$(uname)" == "Darwin" ]]; then HOST_TAG=darwin-x86_64; fi

find_clang() {
  local arch="$1"   # aarch64 or armv7a
  local triple
  if [[ "$arch" == "aarch64" ]]; then
    triple="aarch64-linux-android${API}-clang++"
  else
    triple="armv7a-linux-androideabi${API}-clang++"
  fi
  local path="$NDK/toolchains/llvm/prebuilt/$HOST_TAG/bin/$triple"
  if [[ -x "$path" ]]; then
    echo "$path"
    return
  fi
  # fallback search
  find "$NDK/toolchains/llvm/prebuilt" -name "$triple" 2>/dev/null | head -1
}

build_one() {
  local abi="$1"
  local arch="$2"
  local out="$OUT_DIR/$abi/libentpass.so"
  local clang
  clang=$(find_clang "$arch")
  if [[ -z "$clang" || ! -x "$clang" ]]; then
    echo "WARN: no clang for $abi – skipping"
    return
  fi
  echo "[*] $abi ← $clang"
  "$clang" -shared -fPIC -O2 -std=c++17 \
    -o "$out" \
    "$SRC" \
    -llog -ldl \
    -Wl,-soname,libentpass.so
  echo "[+] $out"
  ls -la "$out"
  file "$out" || true
}

build_one "arm64-v8a"   "aarch64"
build_one "armeabi-v7a" "armv7a"

echo
echo "Done. Libraries are under out/<abi>/libentpass.so"
echo "Copy the ABI(s) that match your APK into lib/<abi>/ inside the unpacked APK."
