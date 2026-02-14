# LiquidDock

A dock plugin for Hyprland with gooey SDF rendering and physics-based animations.

## Features

- Bottom-oriented dock with icons for active and pinned applications
- Custom button system — create icons that execute any terminal command
- macOS-style magnification effect on hover
- Gooey SDF (Signed Distance Field) rendering for a unique "blobbing" visual effect
- Physics-based, spring-driven animations for all dynamic properties
- Auto-hide mode
- Running application indicators
- Right-click to pin/unpin applications
- Click to focus running apps or launch pinned apps

## Example config

```ini
plugin {
    liquiddock {
        enabled = true

        dock_height = 64
        dock_padding = 8
        dock_color = rgba(1a1a1aDD)
        dock_radius = 16
        dock_blur = true

        icon_size = 48
        icon_spacing = 8
        indicator_color = rgba(ffffffCC)

        auto_hide = false
        magnification = true
        magnification_scale = 1.5

        gooey_effect = true
        gooey_threshold = 15.0

        # Pin apps to the dock:
        # liquiddock-pin = appId, command, displayName
        liquiddock-pin = firefox, firefox, Firefox
        liquiddock-pin = kitty, kitty, Kitty Terminal
        liquiddock-pin = nautilus, nautilus, Files
    }
}
```

## Config

All config options are in `plugin:liquiddock`.

| property | type | description | default |
| --- | --- | --- | --- |
| `enabled` | bool | enable the dock | `true` |
| `dock_height` | int | dock bar height in px | `64` |
| `dock_padding` | int | horizontal padding inside the dock | `8` |
| `dock_color` | color | dock background color | `rgba(1a1a1aDD)` |
| `dock_radius` | int | dock corner radius | `16` |
| `dock_blur` | bool | enable blur behind the dock (requires global blur enabled) | `true` |
| `icon_size` | int | icon size in px | `48` |
| `icon_spacing` | int | spacing between icons in px | `8` |
| `indicator_color` | color | running app indicator dot color | `rgba(ffffffCC)` |
| `auto_hide` | bool | auto-hide the dock when not hovered | `false` |
| `magnification` | bool | enable macOS-style magnification on hover | `true` |
| `magnification_scale` | float | max magnification scale factor | `1.5` |
| `gooey_effect` | bool | enable gooey SDF blobbing effect | `true` |
| `gooey_threshold` | float | controls how much icon shapes merge together | `15.0` |

## Pinning Apps

Use the `liquiddock-pin` keyword inside `plugin { liquiddock { } }`.

```ini
liquiddock-pin = appId, command, displayName
```

- `appId` — the application class name (required)
- `command` — command to run when the icon is clicked (defaults to appId)
- `displayName` — label for tooltips (defaults to appId)

## Mouse actions

- **Left-click** an icon: Focus the running app, or launch the pinned app
- **Right-click** an icon: Toggle pin/unpin

## Gooey SDF Rendering

LiquidDock uses a custom GLSL fragment shader that represents each dock icon as a Signed Distance Field (SDF) rounded rectangle. When icons are close together, their SDFs are combined using a smooth minimum function, creating a "gooey" or "blobbing" visual effect where shapes appear to merge and separate fluidly.

The `gooey_threshold` parameter controls the strength of this effect — higher values create more pronounced merging between icons.
