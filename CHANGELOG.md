# Changelog

All notable changes to Floating Mode are documented here.

## 1.0.8 — 2026-08-22

- Made bar clicks use the helper's atomic `toggle` operation instead of stale asynchronously polled UI state
- Cancelled pre-action status polls and forced a fresh status read after each transition
- Serialized UI actions and background synchronization with a private runtime lock

## 1.0.7 — 2026-08-22

- Bounded recurring Hyprland client IPC to 1 MiB and 256 objects
- Bounded monitor IPC to 256 KiB and 64 objects
- Rejected truncated, invalid, non-array, and excessive IPC responses before shell-variable allocation
- Reduced the synchronization polling rate from 700 ms to 1 second

## 1.0.6 — 2026-08-22

- Made the reviewed 40-character hyprland-plugins commit literal at every fetch, checkout, verification, and registration site so automated validation can prove the source pin

## 1.0.5 — 2026-08-22

- Removed the shared `/tmp` fallback for runtime state
- Required private, user-owned runtime and state directories
- Rejected symlinked, non-regular, wrong-owner, and hard-linked state files
- Restricted runtime state files to mode 0600

## 1.0.4 — 2026-08-21

- Restored circular hover backgrounds while keeping controls visible
- Pinned the patched hyprbars build to reviewed upstream commit `7644cecdb947060682891a0db2a0cdc5c0b9e704`
- Verifies the detached source checkout before compiling

## 1.0.3 — 2026-08-21

- Kept maximize and close controls permanently visible

## 1.0.2 — 2026-08-21

- Enabled hyprbars' official `icon_on_hover` button effect

## 1.0.1 — 2026-08-21

- Removed the custom native hyprbars patch and privileged binary replacement
- Delegated installation of the unmodified ABI-pinned hyprbars plugin to hyprpm
- Switched titlebar controls to hyprbars' standard `add_button` interface only
- Removed the custom hover-background options

## 1.0.0 — 2026-08-21

- Added an Omarchy bar toggle for tiled and floating modes
- Added automatic handling of windows opened while Floating Mode is active
- Added scale-aware, centered geometry for lone and newly opened windows
- Preserved tiled placement when converting populated workspaces
- Added instant animation-free window transitions
- Added native draggable titlebars through the official Hyprland `hyprbars` plugin
- Added maximize and close controls with a red circular close-hover state
- Added reversible fullscreen cleanup when returning to tiling
- Added multi-monitor, transformed-monitor, and reserved-area support
- Added a dependency-checking installer with ABI-pinned builds and safe restoration
