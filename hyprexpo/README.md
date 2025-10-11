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

        gesture_distance = 200 # how far is the "max" for the gesture
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
        # rounded corners for workspace tiles
        tile_rounding = 0            # px
        tile_rounding_power = 2.0
        # optional state overrides
        tile_rounding_focus = -1      # -1 = inherit
        tile_rounding_current = -1

        gaps_out = 0
        # numbers (labels)
        label_enable = 1
        label_font_size = 16
        # content mode: token (default) | id | index
        label_text_mode = token
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
        label_bg_enable = 1
        label_bg_color = rgba(00000088)
        label_bg_shape = circle     # circle|square|rounded
        label_bg_rounding = 8       # used for rounded
        label_padding = 4
        # font + precision
        label_font_family = Sans
        label_font_bold = 0
        label_font_italic = 0
        label_text_underline = 0
        label_text_strikethrough = 0
        label_pixel_snap = 1
        # optional: override tokens (up to 50, comma-separated). Empty entries allowed.
        # label_token_map = "1,2,3,4,5,6,7,8,9,0,!,@,#,$,%,^,&,*,(,),a,b,c,..."
    }
}
```

### Configuration

#### Layout & Display

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:columns` | int | how many desktops per row | `3` |
| `plugin:hyprexpo:gaps_in` | int | inner gaps between tiles (px) | `5` |
| `plugin:hyprexpo:gaps_out` | int | outer margin around the grid (px) | `0` |
| `plugin:hyprexpo:bg_col` | color | grid background color | `rgb(111111)` |
| `plugin:hyprexpo:workspace_method` | string | placement: `center current` or `first <ws>` | `center current` |
| `plugin:hyprexpo:skip_empty` | bool (int) | skip empty workspaces (`1`) or not (`0`) | `0` |
| `plugin:hyprexpo:gesture_distance` | int | swipe distance considered "max" (px) | `200` |

#### Tile Appearance

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:tile_rounding` | int | corner radius (px) | `0` |
| `plugin:hyprexpo:tile_rounding_power` | float | rounding power (curve exponent) | `2.0` |
| `plugin:hyprexpo:tile_rounding_focus` | int | focus tile radius (`-1` = inherit) | `-1` |
| `plugin:hyprexpo:tile_rounding_current` | int | current tile radius (`-1` = inherit) | `-1` |
| `plugin:hyprexpo:border_style` | string | `simple` \| `hypr` \| `hyprland` | `simple` |
| `plugin:hyprexpo:border_width` | int | border thickness (px) | `2` |
| `plugin:hyprexpo:border_color_current` | color | color for current tile border (fallback) | `rgb(66ccff)` |
| `plugin:hyprexpo:border_color_focus` | color | color for focus tile border (fallback) | `rgb(ffcc66)` |
| `plugin:hyprexpo:border_grad_current` | string | hyprland gradient for current, e.g. `rgba(33ccffee) rgba(00ff99ee) 45deg` | empty |
| `plugin:hyprexpo:border_grad_focus` | string | hyprland gradient for focus | empty |

#### Labels

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:label_enable` | bool (int) | enable labels | `1` |
| `plugin:hyprexpo:label_text_mode` | string | `token` | `index` | `id` | `token` |
| `plugin:hyprexpo:label_token_map` | string | override up to 50 tokens (comma‑sep). Empty entries skip | empty |
| `plugin:hyprexpo:label_font_size` | int | base font size (px) | `16` |
| `plugin:hyprexpo:label_position` | string | `top-left` | `top-right` | `bottom-left` | `bottom-right` | `center` | `center` |
| `plugin:hyprexpo:label_offset_x` | int | offset from position anchor (px) | `0` |
| `plugin:hyprexpo:label_offset_y` | int | offset from position anchor (px) | `0` |
| `plugin:hyprexpo:label_show` | string | `always` | `hover` | `focus` | `hover+focus` | `current+focus` | `never` | `always` |
| `plugin:hyprexpo:label_color_default` | color | default label color | `rgb(ffffff)` |
| `plugin:hyprexpo:label_color_hover` | color | hover label color | `rgb(eeeeee)` |
| `plugin:hyprexpo:label_color_focus` | color | focus label color | `rgb(ffcc66)` |
| `plugin:hyprexpo:label_color_current` | color | current label color | `rgb(66ccff)` |
| `plugin:hyprexpo:label_scale_hover` | float | scale multiplier on hover | `1.0` |
| `plugin:hyprexpo:label_scale_focus` | float | scale multiplier on focus | `1.0` |
| `plugin:hyprexpo:label_bg_enable` | bool (int) | draw background “bubble” | `1` |
| `plugin:hyprexpo:label_bg_shape` | string | `circle` | `square` | `rounded` | `circle` |
| `plugin:hyprexpo:label_bg_rounding` | int | radius for `rounded` shape (px) | `8` |
| `plugin:hyprexpo:label_bg_color` | color | bubble color | `rgba(00000088)` |
| `plugin:hyprexpo:label_padding` | int | bubble padding (px) | `8` |
| `plugin:hyprexpo:label_font_family` | string | Pango font family | `Sans` |
| `plugin:hyprexpo:label_font_bold` | bool (int) | bold | `0` |
| `plugin:hyprexpo:label_font_italic` | bool (int) | italic | `0` |
| `plugin:hyprexpo:label_text_underline` | bool (int) | underline | `0` |
| `plugin:hyprexpo:label_text_strikethrough` | bool (int) | strikethrough | `0` |
| `plugin:hyprexpo:label_pixel_snap` | bool (int) | snap to integer pixels | `1` |
| `plugin:hyprexpo:label_center_adjust_x` | int | manual center nudge X (px) | `0` |
| `plugin:hyprexpo:label_center_adjust_y` | int | manual center nudge Y (px) | `0` |

#### Keyboard Navigation

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:keynav_enable` | bool (int) | enable keynav + auto submap | `1` |
| `plugin:hyprexpo:keynav_wrap_h` | bool (int) | wrap horizontally | `1` |
| `plugin:hyprexpo:keynav_wrap_v` | bool (int) | wrap vertically | `1` |
| `plugin:hyprexpo:keynav_reading_order` | bool (int) | use row‑major for horizontal moves | `0` |

#### Scrolling Overview (Layout-Specific)

When using Hyprland's scrolling layout, HyprExpo automatically switches to a vertical scrolling overview mode.

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:scrolling:scroll_moves_up_down` | bool (int) | if enabled, mouse wheel scrolls through workspaces; if disabled, mouse wheel zooms | `1` |
| `plugin:hyprexpo:scrolling:default_zoom` | float | default zoom level on overview open (0.1 - 0.9) | `0.5` |

**Note:** Scrolling overview mode does not support keyboard navigation. Mouse/trackpad interaction is the primary input method.

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

# Optional submap for keyboard selection when Hyprexpo is active (index-based, 1..46)
submap = hyprexpo
    bind = , left,  hyprexpo:kb_focus, left
    bind = , right, hyprexpo:kb_focus, right
    bind = , up,    hyprexpo:kb_focus, up
    bind = , down,  hyprexpo:kb_focus, down
    bind = , return, hyprexpo:kb_confirm

    # 1..10 via digits (0 -> 10)
    bind = , 1, hyprexpo:kb_selecti, 1
    bind = , 2, hyprexpo:kb_selecti, 2
    bind = , 3, hyprexpo:kb_selecti, 3
    bind = , 4, hyprexpo:kb_selecti, 4
    bind = , 5, hyprexpo:kb_selecti, 5
    bind = , 6, hyprexpo:kb_selecti, 6
    bind = , 7, hyprexpo:kb_selecti, 7
    bind = , 8, hyprexpo:kb_selecti, 8
    bind = , 9, hyprexpo:kb_selecti, 9
    bind = , 0, hyprexpo:kb_selecti, 10

    # 11..20 via SHIFT+digits
    bind = SHIFT, 1, hyprexpo:kb_selecti, 11
    bind = SHIFT, 2, hyprexpo:kb_selecti, 12
    bind = SHIFT, 3, hyprexpo:kb_selecti, 13
    bind = SHIFT, 4, hyprexpo:kb_selecti, 14
    bind = SHIFT, 5, hyprexpo:kb_selecti, 15
    bind = SHIFT, 6, hyprexpo:kb_selecti, 16
    bind = SHIFT, 7, hyprexpo:kb_selecti, 17
    bind = SHIFT, 8, hyprexpo:kb_selecti, 18
    bind = SHIFT, 9, hyprexpo:kb_selecti, 19
    bind = SHIFT, 0, hyprexpo:kb_selecti, 20

    # 21..46 via alpha
    bind = , a, hyprexpo:kb_selecti, 21
    bind = , b, hyprexpo:kb_selecti, 22
    bind = , c, hyprexpo:kb_selecti, 23
    bind = , d, hyprexpo:kb_selecti, 24
    bind = , e, hyprexpo:kb_selecti, 25
    bind = , f, hyprexpo:kb_selecti, 26
    bind = , g, hyprexpo:kb_selecti, 27
    bind = , h, hyprexpo:kb_selecti, 28
    bind = , i, hyprexpo:kb_selecti, 29
    bind = , j, hyprexpo:kb_selecti, 30
    bind = , k, hyprexpo:kb_selecti, 31
    bind = , l, hyprexpo:kb_selecti, 32
    bind = , m, hyprexpo:kb_selecti, 33
    bind = , n, hyprexpo:kb_selecti, 34
    bind = , o, hyprexpo:kb_selecti, 35
    bind = , p, hyprexpo:kb_selecti, 36
    bind = , q, hyprexpo:kb_selecti, 37
    bind = , r, hyprexpo:kb_selecti, 38
    bind = , s, hyprexpo:kb_selecti, 39
    bind = , t, hyprexpo:kb_selecti, 40
    bind = , u, hyprexpo:kb_selecti, 41
    bind = , v, hyprexpo:kb_selecti, 42
    bind = , w, hyprexpo:kb_selecti, 43
    bind = , x, hyprexpo:kb_selecti, 44
    bind = , y, hyprexpo:kb_selecti, 45
    bind = , z, hyprexpo:kb_selecti, 46
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
- `hyprexpo:kb_selectn, <id>`: selects by workspace id (legacy; `0` → `10`).
- `hyprexpo:kb_selecti, <index>`: selects by 1‑based visual index (recommended for single‑keystroke mapping).
- `hyprexpo:kb_select, <token>`: selects by a single token (1..9, 0, a..z), mainly for symbol‑based configs.

Border styles:
- `simple`: single-color border using `border_width`, `border_color_current`, `border_color_focus`.
- `hypr`: layered border approximating Hyprland’s gradient borders by drawing 3 layers (lightened, base, and darkened). Uses the same base colors and splits the width across layers.
- `hyprland`: 2-color gradient with angle. Provide gradients:
  - `plugin:hyprexpo:border_grad_current = rgba(33ccffee) rgba(00ff99ee) 45deg`
  - `plugin:hyprexpo:border_grad_focus   = rgba(ffdd44ee) rgba(22aaffee) 30deg`

Gaps:
- `gaps_in` controls the inner spacing between tiles.
- `gaps_out` adds an outer margin (px) around the grid (animated during open/close).

Labels (numbers):
- `label_position`, `label_offset_x`, `label_offset_y` control placement per tile.
- `label_show` controls when labels are drawn (always/hover/focus/etc.).
- Per-state colors: `label_color_default|hover|focus|current`.
- Per-state scale multipliers: `label_scale_hover|focus`.
- Optional background bubble behind text: `label_bg_*`, `label_padding`.
- Content:
  - `label_text_mode = token` (default) uses the token sequence (1..9,0,!,@,#,…,a..z) or `label_token_map` if provided.
  - `label_text_mode = index` shows the 1-based visual index.
  - `label_text_mode = id` shows the workspace ID.
- Token overrides:
  - `label_token_map` accepts up to 50 comma-separated entries to override tokens. Empty entries allowed (skip rendering).
- Font & precision:
  - `label_font_family`, `label_font_bold`, `label_font_italic`, `label_text_underline`, `label_text_strikethrough`
  - `label_pixel_snap` (default 1) rounds positions to integer pixels for crisper text.
