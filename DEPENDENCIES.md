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
| `git` | Clones that exact official source commit and applies the bundled patch |
| `make`, `gcc`, `g++`, `pkg-config` | Builds `hyprbars.so` |
| `cmake`, `cpio` | Required by `hyprpm` for headers and plugin management |
| `sudo`, `install` | Replaces and restores only the cached `hyprbars.so` |
| `sed`, `grep`, `awk`, `mktemp` | Installer validation, metadata parsing, and temporary workspace handling |

The installer verifies these commands before beginning. It needs network access only for `hyprpm update` and the clone of `https://github.com/hyprwm/hyprland-plugins`.

## Files and privileges

Normal user writes:

- `~/.config/hypr/floating-mode.lua`
- One exact `require("hypr.floating-mode")` line in `~/.config/hypr/hyprland.lua`
- `$XDG_STATE_HOME/omarchy-floating-mode/` for the original module and previous enabled state
- `$XDG_RUNTIME_DIR/omarchy-floating-mode/` for session-only window state

Privileged write:

- `/var/cache/hyprpm/$USER/hyprland-plugins/hyprbars.so`

The original cached module is saved before replacement. `--uninstall` restores it, restores the previous enabled state, removes the Lua integration, reloads Hyprland, and deletes the installer state.

## External code

- Official upstream: [hyprwm/hyprland-plugins](https://github.com/hyprwm/hyprland-plugins)
- Modified component: `hyprbars` only
- Local modification: [`patches/hyprbars-button-hover.patch`](patches/hyprbars-button-hover.patch)
- Patch purpose: configurable circular hover backgrounds for standard and close buttons

The upstream source is not vendored in this repository. The installer checks out the commit recorded by `hyprpm`, so the build follows Hyprland's ABI pin instead of the moving upstream branch.
