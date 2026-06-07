# z13-dwt

Disable tap-to-click while typing on ASUS ROG Flow Z13 keyboard covers under
KDE Plasma/KWin.

By default `z13-dwt` discovers the Flow Z13 keyboard-cover touchpad and keyboard
event devices from `/proc/bus/input/devices`, disables KWin `tapToClick` while
typing, and enables `tapToClick` again after a quiet period.

## Requirements

- Linux with `systemd-logind`
- KDE Plasma/KWin with `org.kde.KWin.InputDevice.tapToClick`
- `busctl`
- root access for `/dev/input/event*`

## Build

```sh
make
```

## Install

```sh
sudo make install
sudo make enable-service
```

After code changes:

```sh
sudo make install
sudo make restart-service
```

Installed files:

- `/usr/local/bin/z13-dwt`
- `/etc/systemd/system/z13-dwt.service`

## Configuration

The built-in auto-detection looks for the `GZ302EA-Keyboard Touchpad` input
device and keyboard-cover devices named `GZ302EA-Keyboard` or `N-KEY Device`.

Environment variables:

- `Z13_DWT_TOUCHPAD`: override touchpad event node, for example
  `/dev/input/event5`.
- `Z13_DWT_QUIET_MS`: milliseconds after the last keypress before tap-to-click
  is enabled again. Default: `700`.
- `Z13_DWT_DEBUG`: set to `1` for state logs.

For systemd overrides:

```sh
sudo systemctl edit z13-dwt.service
```

Example:

```ini
[Service]
Environment=Z13_DWT_QUIET_MS=700
```

## Manual Run

Use defaults and auto-discovered keyboards:

```sh
sudo ./z13-dwt
```

Or specify devices explicitly:

```sh
sudo ./z13-dwt /dev/input/event-touchpad /dev/input/event-keyboard [...]
```

## Status

```sh
systemctl status z13-dwt.service
journalctl -u z13-dwt.service -n 50
```

At startup the journal logs the selected touchpad, KWin object path, quiet
period, keyboard candidates, and opened keyboard devices.
