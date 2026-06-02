# z13-dwt

Disable-while-typing helper for KDE Plasma Wayland systems where libinput does
not expose native DWT for the touchpad.

This was written for the ASUS ROG Flow Z13 keyboard cover, whose touchpad can
register palm taps while typing. The daemon reads keyboard events as root and
temporarily disables KWin's `tapToClick` setting for the touchpad. After the
quiet period expires, it restores whatever `tapToClick` value was active before
the suppression cycle began.

## What It Does

- Watches one touchpad event node and one or more keyboard event nodes.
- On the first keypress after idle, reads the current KWin `tapToClick` value.
- Sets `tapToClick=false`.
- Extends the timer while typing continues.
- Restores the saved value after the quiet period, 700 ms by default.
- Resolves the active local graphical user through `systemd-logind`.
- Drops privileges for KWin D-Bus calls instead of talking to the user's bus as
  root.

## Requirements

- Linux
- KDE Plasma/KWin with the `org.kde.KWin.InputDevice.tapToClick` D-Bus property
- `systemd-logind`
- `busctl`
- root access for reading `/dev/input/event*`

This is not a general libinput DWT implementation. It is a pragmatic KDE/KWin
workaround for devices where native DWT is unavailable.

## Build

```sh
make
```

## Manual Run

The current launcher is tailored to the ROG Flow Z13 keyboard cover:

```sh
sudo ./scripts/run-z13-dwt.sh
```

For other devices, call the binary directly:

```sh
sudo ./z13-dwt /dev/input/event-touchpad /dev/input/event-keyboard [...]
```

## Install On This Machine

The included installer writes a root systemd service that starts the launcher:

```sh
sudo bash ./scripts/install-z13-dwt-system.sh
```

The installer builds the binary, installs it to `/usr/local/bin/z13-dwt`,
installs the launcher to `/usr/local/libexec/z13-dwt/run-z13-dwt.sh`, and writes
`/etc/systemd/system/z13-dwt.service`.

Check status:

```sh
systemctl status z13-dwt.service
journalctl -u z13-dwt.service -n 50
```

## Device Notes

On the Flow Z13 tested here:

- touchpad: `/dev/input/by-id/usb-ASUSTeK_Computer_Inc._GZ302EA-Keyboard-if03-event-mouse`
- keyboard sources are discovered from `/proc/bus/input/devices`

If your device has a different touchpad path, set `Z13_DWT_TOUCHPAD` in the
service environment or invoke the binary directly.

## Configuration

Environment variables:

- `Z13_DWT_TOUCHPAD`: touchpad event node or stable `/dev/input/by-id/...` path.
- `Z13_DWT_BIN`: binary path used by `scripts/run-z13-dwt.sh`.
- `Z13_DWT_QUIET_MS`: suppression period after the last key event. Default:
  `700`. Accepted range: `0` to `10000`.
- `Z13_DWT_DEBUG`: set to `1` for state-transition logging.

## Debugging

The daemon caches the active KWin target session. If a cached D-Bus call fails,
it re-queries `loginctl`, so logout/login or user switching should self-heal
after at most one missed toggle.
