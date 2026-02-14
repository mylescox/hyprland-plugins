# hyprpill

Adds an animated pill-shaped window grabber above each window for quick focusing and drag-to-move interactions.

The visible pill is intentionally smaller than its interaction zone, so grabbing is easier.

## Pill mouse actions

- Left-drag moves the window (existing behavior).
- Left double-click toggles floating for the pill's window (uses `double_click_timeout`).
- Middle-click closes the pill's window (`killactive`).
- Right-click toggles pseudo mode only when the window is currently tiled.


## Example config

```ini
plugin {
  hyprpill {
    enabled = true

    pill_width = 100
    pill_height = 12
    pill_radius = 12
    pill_width_inactive = 26
    pill_height_inactive = 4
    pill_radius_inactive = 2
    pill_width_hover = 150
    pill_height_hover = 12

    hover_hitbox_width = 40
    hover_hitbox_height = 20
    hover_hitbox_offset_y = -9
    dodge_occluder_margin = 4

    click_hitbox_width = 35
    click_hitbox_height = 15
    click_hitbox_offset_y = -4

    debug_hitbox_hover = false
    debug_hitbox_click = false
    debug_cursor_state = false

    hover_cursor = hand1
    grab_cursor = hand2
    drag_pixel_threshold = 8
    double_click_timeout = 250

    pill_color_active = rgba(ffffff77)
    pill_color_inactive = rgba(777777FF)
    pill_color_hover = rgba(ffffffAA)
    pill_color_pressed = rgba(ffffffff)

    pill_opacity_active = 1.0
    pill_opacity_inactive = 0.2

    pill_offset_y_active = 20
    pill_offset_y_inactive = 8

    anim_duration_hover = 100
    anim_duration_press = 120
    anim_easing = easeOutExpo
    geometry_lerp_speed = 150
    geometry_lerp_easing = easeInOut

    pill_blur = false
    pill_part_of_window = false
    pill_precedence_over_border = true
  }
}
```

## Config

All config options are in `plugin:hyprpill`.

| property | type | description | default |
| --- | --- | --- | --- |
| `enabled` | bool | enable the plugin | `true` |
| `pill_width` | int | pill width in px | `100` |
| `pill_height` | int | pill thickness/height in px | `12` |
| `pill_radius` | int | corner radius | `12` |
| `pill_width_inactive` | int | width when inactive/unfocused | `26` |
| `pill_height_inactive` | int | height when inactive/unfocused | `4` |
| `pill_radius_inactive` | int | corner radius when inactive/unfocused | `2` |
| `pill_width_hover` | int | width when hovered/pressed | `150` |
| `pill_height_hover` | int | height when hovered/pressed | `12` |
| `hover_hitbox_width` | int | extra horizontal hover hitbox padding | `40` |
| `hover_hitbox_height` | int | extra vertical hover hitbox padding | `20` |
| `hover_hitbox_offset_y` | int | hover hitbox y offset from visible pill | `-9` |
| `dodge_occluder_margin` | int | extra horizontal padding used when avoiding top-edge occluding windows | `4` |
| `click_hitbox_width` | int | extra horizontal click/drag hitbox padding | `35` |
| `click_hitbox_height` | int | extra vertical click/drag hitbox padding | `15` |
| `click_hitbox_offset_y` | int | click hitbox y offset from visible pill | `-4` |
| `debug_hitbox_hover` | bool | draw hover hitbox overlay for tuning | `false` |
| `debug_hitbox_click` | bool | draw click hitbox overlay for tuning | `false` |
| `debug_cursor_state` | bool | draw a state indicator showing whether hyprpill expects default/hover/grab cursor | `false` |
| `hover_cursor` | str | cursor shape name while hovering the hover hitbox (`""` to disable override) | `hand1` |
| `grab_cursor` | str | cursor shape name while click/drag is active (`""` to disable override) | `hand2` |
| `drag_pixel_threshold` | int | movement threshold in px before a press turns into a window drag | `8` |
| `double_click_timeout` | int | max time in ms between left clicks to trigger pill double-click actions | `250` |
| `pill_color_active` | color | active/focused color | `rgba(ffffff77)` |
| `pill_color_inactive` | color | inactive color | `rgba(777777FF)` |
| `pill_color_hover` | color | hover color | `rgba(ffffffAA)` |
| `pill_color_pressed` | color | pressed/dragging color | `rgba(ffffffff)` |
| `pill_opacity_active` | float | active opacity | `1.0` |
| `pill_opacity_inactive` | float | inactive opacity | `0.2` |
| `pill_offset_y_active` | int | y offset for focused window | `20` |
| `pill_offset_y_inactive` | int | y offset for inactive window | `8` |
| `anim_duration_hover` | int | hover animation duration (ms) | `100` |
| `anim_duration_press` | int | press/drag animation duration (ms) | `120` |
| `anim_easing` | str | animation easing selector | `easeOutExpo` |
| `geometry_lerp_speed` | float | speed multiplier for dodge geometry lerp (x/y/w/h) | `150` |
| `geometry_lerp_easing` | str | easing for dodge geometry lerp (`linear`, `easeIn`, `easeOut`, `easeInOut`) | `easeInOut` |
| `pill_blur` | bool | enable blur pass integration | `false` |
| `pill_part_of_window` | bool | include pill in main window extents | `false` |
| `pill_precedence_over_border` | bool | draw above border decoration | `true` |

## Dynamic window rules

`hyprpill:no_pill` disables pill for matching windows.

`hyprpill:pill_color` overrides the pill color for matching windows.

Examples:

```ini
windowrule = plugin:hyprpill:no_pill 1, class:^(steam)$
windowrule = plugin:hyprpill:pill_color rgba(ff7f7fff), class:^(kitty)$
```
