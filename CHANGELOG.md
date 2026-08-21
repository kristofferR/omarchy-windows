# Changelog

All notable changes to Floating Mode are documented here.

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
