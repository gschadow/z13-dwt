#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "run as root, for example: sudo bash $0" >&2
  exit 1
fi

make -C "$repo_root" install enable-service restart-service

echo "Installed z13-dwt binary, launcher, and systemd unit"
echo "Started z13-dwt.service"
