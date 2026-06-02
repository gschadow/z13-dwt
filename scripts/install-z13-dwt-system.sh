#!/usr/bin/env bash
set -euo pipefail

unit=/etc/systemd/system/z13-dwt.service
script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
install_bin=/usr/local/bin/z13-dwt
install_dir=/usr/local/libexec/z13-dwt
install_launcher=$install_dir/run-z13-dwt.sh

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "run as root, for example: sudo bash $0" >&2
  exit 1
fi

make -C "$repo_root"
install -Dm755 "$repo_root/z13-dwt" "$install_bin"
install -Dm755 "$script_dir/run-z13-dwt.sh" "$install_launcher"

cat >"$unit" <<EOF
[Unit]
Description=Z13 disable while typing
After=multi-user.target

[Service]
Type=simple
ExecStart=$install_launcher
Restart=always
RestartSec=1

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable --now z13-dwt.service
systemctl restart z13-dwt.service

echo "Installed $install_bin, $install_launcher, and $unit"
echo "Started z13-dwt.service"
