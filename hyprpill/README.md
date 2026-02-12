# hyprpill

Adds an animated pill-shaped window grabber above each window for quick focusing and drag-to-move interactions.

The visible pill is intentionally smaller than its interaction zone, so grabbing is easier.

## Example config

```ini
plugin {
  hyprpill {
    enabled = true

    pill_width = 96
    pill_height = 12
    pill_radius = 12
    pill_width_hover = 120
    pill_height_hover = 14

    pill_hitbox_width = 10
    pill_hitbox_height = 8

    pill_color_active = rgba(99ccffff)
    pill_color_inactive = rgba(777777cc)
    pill_color_hover = rgba(bbe6ffff)
    pill_color_pressed = rgba(66b2ffff)

    pill_opacity_active = 1.0
    pill_opacity_inactive = 0.6

    pill_offset_y_active = 4
    pill_offset_y_inactive = 2

    anim_duration_hover = 140
    anim_duration_press = 100
    anim_easing = easeInOut

    pill_blur = false
    pill_part_of_window = false
    pill_precedence_over_border = false
  }
}
```

## Config

All config options are in `plugin:hyprpill`.

| property | type | description | default |
| --- | --- | --- | --- |
| `enabled` | bool | enable the plugin | `true` |
| `pill_width` | int | pill width in px | `96` |
| `pill_height` | int | pill thickness/height in px | `12` |
| `pill_radius` | int | corner radius | `12` |
| `pill_width_hover` | int | width when hovered/pressed | `120` |
| `pill_height_hover` | int | height when hovered/pressed | `14` |
| `pill_hitbox_width` | int | extra horizontal hitbox padding | `10` |
| `pill_hitbox_height` | int | extra vertical hitbox padding | `8` |
| `pill_color_active` | color | active/focused color | `rgba(99ccffff)` |
| `pill_color_inactive` | color | inactive color | `rgba(777777cc)` |
| `pill_color_hover` | color | hover color | `rgba(bbe6ffff)` |
| `pill_color_pressed` | color | pressed/dragging color | `rgba(66b2ffff)` |
| `pill_opacity_active` | float | active opacity | `1.0` |
| `pill_opacity_inactive` | float | inactive opacity | `0.6` |
| `pill_offset_y_active` | int | y offset for focused window | `4` |
| `pill_offset_y_inactive` | int | y offset for inactive window | `2` |
| `anim_duration_hover` | int | hover animation duration (ms) | `140` |
| `anim_duration_press` | int | press/drag animation duration (ms) | `100` |
| `anim_easing` | str | animation easing selector | `easeInOut` |
| `pill_blur` | bool | enable blur pass integration | `false` |
| `pill_part_of_window` | bool | include pill in main window extents | `false` |
| `pill_precedence_over_border` | bool | draw above border decoration | `false` |

## Dynamic window rules

`hyprpill:no_pill` disables pill for matching windows.

`hyprpill:pill_color` overrides the pill color for matching windows.

Examples:

```ini
windowrule = plugin:hyprpill:no_pill 1, class:^(steam)$
windowrule = plugin:hyprpill:pill_color rgba(ff7f7fff), class:^(kitty)$
```
