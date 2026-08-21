# Dependency audit

Floating Mode has no vendored libraries, package-manager install hooks, telemetry, or runtime network dependency.

## Runtime dependencies

| Dependency | Purpose | Expected source |
| --- | --- | --- |
| Omarchy 4 / Omarchy Shell | Loads the service and bar widget | Standard Omarchy installation |
| Hyprland 0.56+ | Window state, geometry, dispatchers, and native plugin ABI | Standard Omarchy installation |
| Quickshell Qt/QML imports | Hosts `BarWidget.qml` and `Service.qml` | Omarchy Shell |
| `hyprctl` | Reads clients and monitors; applies window transitions | Hyprland package |
| `jq` | Safely calculates logical monitor and window geometry | Omarchy base packages |

Runtime operation is local and uses the current user's permissions.

## Explicit one-time installer dependencies

| Dependency | Purpose |
| --- | --- |
| `hyprpm` | Selects the official `hyprbars` commit compatible with the installed Hyprland ABI |
| `install`, `sed`, `grep`, `awk` | Installs user configuration and tracks the previous enabled state |

The installer verifies these commands before beginning. It needs network access only when `hyprpm` updates or adds the official plugin repository.

## Files and privileges

Normal user writes:

- `~/.config/hypr/floating-mode.lua`
- One exact `require("hypr.floating-mode")` line in `~/.config/hypr/hyprland.lua`
- `$XDG_STATE_HOME/omarchy-floating-mode/` for the previous enabled state
- `$XDG_RUNTIME_DIR/omarchy-floating-mode/` for session-only window state

Floating Mode performs no privileged writes and does not invoke `sudo`. `--uninstall` restores the previous enabled state, removes the Lua integration, reloads Hyprland, and deletes the installer state.

## External code

- Official upstream: [hyprwm/hyprland-plugins](https://github.com/hyprwm/hyprland-plugins)
- Interface used: hyprbars' standard configuration and `add_button` action

The upstream source is not vendored, cloned, or patched by Floating Mode. Native plugin installation is delegated entirely to Hyprland's official `hyprpm` manager and its ABI commit pins.
