# z13-dwt

Disable tap-to-click while typing on ASUS ROG Flow Z13 keyboard covers under
KDE Plasma/KWin.

By default `z13-dwt` uses the tested Flow Z13 touchpad path, discovers keyboard
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

Default touchpad path:

```sh
/dev/input/by-id/usb-ASUSTeK_Computer_Inc._GZ302EA-Keyboard-if03-event-mouse
```

Environment variables:

- `Z13_DWT_TOUCHPAD`: touchpad event node or `/dev/input/by-id/...` path.
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
