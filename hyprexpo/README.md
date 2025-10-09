# HyprExpo
HyprExpo is an overview plugin like Gnome, KDE or wf.
  
![HyprExpo](https://github.com/user-attachments/assets/e89df9d2-9800-4268-9929-239ad9bc3a54)
  
## Config
A great start to configure this plugin would be adding this code to the `plugin` section of your hyprland configuration file:  
```ini
# .config/hypr/hyprland.conf
plugin {
    hyprexpo {
        columns = 3
        gaps_in = 5
        bg_col = rgb(111111)
        workspace_method = center current # [center/first] [workspace] e.g. first 1 or center m+1

        gesture_distance = 300 # how far is the "max" for the gesture
        gaps_out = 0 # outer margin (px)
    }
}
```

Keyboard navigation (optional):
```ini
# Enable keyboard navigation + numbering and borders
plugin {
    hyprexpo {
        keynav_enable = 1
        keynav_wrap_h = 1           # wrap horizontally at row edges
        keynav_wrap_v = 1           # wrap vertically at column edges
        # set to 1 to enable row-major horizontal moves
        keynav_reading_order = 0
        border_style = simple         # or hypr (multi-layer) or hyprland (2-color + angle)
        border_width = 2
        border_color_current = rgb(66ccff)
        border_color_focus = rgb(ffcc66)

        gaps_out = 0
        # numbers (labels)
        label_enable = 1
        label_font_size = 16
        # positioning
        label_position = top-left   # top-left|top-right|bottom-left|bottom-right|center
        label_offset_x = 6
        label_offset_y = 6
        # visibility behavior
        label_show = always         # always|hover|focus|hover+focus|current+focus|never
        # colors (per state)
        label_color_default = rgb(ffffff)
        label_color_hover   = rgb(eeeeee)
        label_color_focus   = rgb(ffcc66)
        label_color_current = rgb(66ccff)
        # scale multipliers for states
        label_scale_hover = 1.0
        label_scale_focus = 1.0
        # label background bubble
        label_bg_enable = 0
        label_bg_color = rgba(00000088)
        label_bg_rounding = 4
        label_padding = 4
    }
}
```

### Properties

| property | type | description | default |
| --- | --- | --- | --- |
| columns | number | how many desktops are displayed on one line | `3` |
| gaps_in | number | inner gaps between tiles | `5` |
| gaps_out | number | outer margin around the grid | `0` |
| bg_col | color | color in gaps (between desktops) | `rgb(000000)` |
| workspace_method | [center/first] [workspace] | position of the desktops | `center current` |
| skip_empty | boolean | whether the grid displays workspaces sequentially by id using selector "r" (`false`) or skips empty workspaces using selector "m" (`true`) | `false` |
| gesture_distance | number | how far is the max for the gesture | `300` |

#### Subcategory `scrolling`

Applies to the scrolling layout overview
| property | type | description | default |
| --- | --- | --- | --- |
| scroll_moves_up_down | bool | if enabled, scrolling will move workspaces up/down instead of zooming | `true` |
| default_zoom | float | default zoom out value, [0.1 - 0.9] | `0.5` |

### Keywords

| name | description | arguments |
| -- | -- | -- | 
| hyprexpo-gesture | same as gesture, but for hyprexpo gestures. Supports: `expo`. | Same as gesture |

### Binding
```bash
# hyprland.conf
bind = MODIFIER, KEY, hyprexpo:expo, OPTION
```

Example:  
```bash
# This will toggle HyprExpo when SUPER+g is pressed
bind = SUPER, g, hyprexpo:expo, toggle

# Optional submap for keyboard selection when Hyprexpo is active
submap = hyprexpo
    binde = , left,  hyprexpo:kb_focus, left
    binde = , right, hyprexpo:kb_focus, right
    binde = , up,    hyprexpo:kb_focus, up
    binde = , down,  hyprexpo:kb_focus, down
    binde = , return, hyprexpo:kb_confirm
    # number selection by workspace id
    binde = , 1, hyprexpo:kb_selectn, 1
    binde = , 2, hyprexpo:kb_selectn, 2
    binde = , 3, hyprexpo:kb_selectn, 3
    binde = , 4, hyprexpo:kb_selectn, 4
    binde = , 5, hyprexpo:kb_selectn, 5
    binde = , 6, hyprexpo:kb_selectn, 6
    binde = , 7, hyprexpo:kb_selectn, 7
    binde = , 8, hyprexpo:kb_selectn, 8
    binde = , 9, hyprexpo:kb_selectn, 9
    binde = , 0, hyprexpo:kb_selectn, 0 # 0 -> 10
submap = reset
```

Here are a list of options you can use:  
| option | description |
| --- | --- |
toggle | displays if hidden, hide if displayed
select | selects the hovered desktop
off | hides the overview
disable | same as `off`
on | displays the overview
enable | same as `on`

Keyboard navigation dispatchers (when overview is active):
- `hyprexpo:kb_focus, <left|right|up|down>`: moves focus across tiles (skips invalid).
- Wrapping can be configured with `keynav_wrap_h` and `keynav_wrap_v`.
- Reading order (row-major) for horizontal movement can be enabled with `keynav_reading_order`. At grid ends it will wrap to start/end only if both `keynav_wrap_h` and `keynav_wrap_v` are enabled.
- `hyprexpo:kb_confirm`: selects the focused tile.
- `hyprexpo:kb_selectn, <digit>`: selects the workspace with that id if visible in the grid (0 → 10).

Border styles:
- `simple`: single-color border using `border_width`, `border_color_current`, `border_color_focus`.
- `hypr`: layered border approximating Hyprland’s gradient borders by drawing 3 layers (lightened, base, and darkened). Uses the same base colors and splits the width across layers.
- `hyprland`: 2-color gradient with angle. Provide gradients:
  - `plugin:hyprexpo:border_grad_current = rgba(33ccffee) rgba(00ff99ee) 45deg`
  - `plugin:hyprexpo:border_grad_focus   = rgba(ffdd44ee) rgba(22aaffee) 30deg`

Gaps:
- `gaps_in` controls the spacing between tiles.
- `gaps_out` adds the same spacing around the outer edge of the grid.

Labels (numbers):
- `label_position`, `label_offset_x`, `label_offset_y` control placement per tile.
- `label_show` controls when labels are drawn (always/hover/focus/etc.).
- Per-state colors: `label_color_default|hover|focus|current`.
- Per-state scale multipliers: `label_scale_hover|focus`.
- Optional background bubble behind text: `label_bg_*`, `label_padding`.
