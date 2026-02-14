# Hyprpill

This repo was forked from hyperland-plugins, but the main purpose for this repo is for development of Hyprpill, a grabbable title bar alternative for Hyprland windows (dwindle). Hyprbars and LiquidDock are also included.

<img width="1630" height="1307" alt="image" src="https://github.com/user-attachments/assets/e2096614-7a6b-4548-b5b4-55022703192f" />

# Features
 - A grabbable pill indicator that elegantly floats above Hyprland windows. Click and drag on the pill to move a window.
 - Pill width, radius, height, color, and opacity can be animated based on active window focus state and mouse hover state.
 - Pills intelligently dodge occluding windows.
 - Double-click pill to toggle floating. Middle-click to kill the window. When the window is tiled, right-click the pill to toggle pseudotile mode.
 - Robust configuration options (see /hyprpill/README.md)

# LiquidDock
 - A dock plugin with gooey SDF rendering and physics-based animations (see /liquiddock/README.md)

## Install with `hyprpm`

To install hyprpill, from the command line run:
```bash
hyprpm update
```
Then add this repository:
```bash
hyprpm add https://github.com/mylescox/hyprland-plugins
```
then enable hyprpill with
```bash
hyprpm enable hyprpill
```

See [the plugins wiki](https://wiki.hyprland.org/Plugins/Using-Plugins/#installing--using-plugins) and `hyprpm -h` for more details.

# Contributing

Feel free to open issues and MRs with fixes.
