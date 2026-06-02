#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"

touchpad="${Z13_DWT_TOUCHPAD:-/dev/input/by-id/usb-ASUSTeK_Computer_Inc._GZ302EA-Keyboard-if03-event-mouse}"
binary="${Z13_DWT_BIN:-}"

if [[ -z "$binary" ]]; then
  if [[ -x /usr/local/bin/z13-dwt ]]; then
    binary=/usr/local/bin/z13-dwt
  else
    binary="$repo_root/z13-dwt"
  fi
fi

if [[ ! -x "$binary" ]]; then
  echo "z13-dwt binary not found or not executable: $binary" >&2
  exit 1
fi

touchpad_event="$(readlink -f "$touchpad")"
touchpad_event="${touchpad_event##*/}"

mapfile -t keyboards < <(
  awk -v skip="$touchpad_event" '
    /^H: Handlers=/ && $0 ~ /kbd/ {
      if (match($0, /event[0-9]+/)) {
        dev = substr($0, RSTART, RLENGTH)
        if (dev != skip)
          print "/dev/input/" dev
      }
    }
  ' /proc/bus/input/devices
)

if [[ ${#keyboards[@]} -eq 0 ]]; then
  echo "no keyboard event devices found in /proc/bus/input/devices" >&2
  exit 1
fi

exec "$binary" "$touchpad" "${keyboards[@]}"
