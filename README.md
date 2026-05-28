# z13-dwt

Disable tap-to-click while typing on ASUS ROG Flow Z13 keyboard covers under
KDE Plasma/KWin.

The daemon reads keyboard events from `/dev/input/event*` as root. When a key is
pressed it sets the KWin touchpad property `tapToClick=false`; after the quiet
period it sets `tapToClick=true`.

## Requirements

- Linux with `systemd-logind`
- KDE Plasma/KWin exposing `org.kde.KWin.InputDevice.tapToClick`
- `busctl`
- root access for `/dev/input/event*`

## Build

```sh
make
```

## Install

Install files:

```sh
sudo make install
```

This installs:

- `/usr/local/bin/z13-dwt`
- `/usr/local/libexec/z13-dwt/run-z13-dwt.sh`
- `/etc/systemd/system/z13-dwt.service`

Enable and start:

```sh
sudo make enable-service
```

Restart after upgrades:

```sh
sudo make restart-service
```

One-command install and start:

```sh
sudo ./scripts/install-z13-dwt-system.sh
```

## Configuration

The launcher defaults to the tested Flow Z13 touchpad path:

```sh
/dev/input/by-id/usb-ASUSTeK_Computer_Inc._GZ302EA-Keyboard-if03-event-mouse
```

Environment variables:

- `Z13_DWT_TOUCHPAD`: touchpad event node or `/dev/input/by-id/...` path.
- `Z13_DWT_QUIET_MS`: milliseconds after the last keypress before tap-to-click
  is re-enabled. Default: `700`.
- `Z13_DWT_DEBUG`: set to `1` for state logs.
- `Z13_DWT_BIN`: override the binary used by `scripts/run-z13-dwt.sh`.

For systemd, set environment overrides with a drop-in:

```sh
sudo systemctl edit z13-dwt.service
```

Example:

```ini
[Service]
Environment=Z13_DWT_QUIET_MS=700
```

## Manual Run

Using the Flow Z13 launcher:

```sh
sudo ./scripts/run-z13-dwt.sh
```

Direct binary invocation:

```sh
sudo ./z13-dwt /dev/input/event-touchpad /dev/input/event-keyboard [...]
```

## Status

```sh
systemctl status z13-dwt.service
journalctl -u z13-dwt.service -n 50
```
