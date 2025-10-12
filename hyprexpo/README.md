> HyprExpo+ may be an ephemeral plugin, it all depends how the [PR to `hyprland-plugins`](https://github.com/hyprwm/hyprland-plugins/pull/507) goes. When/if PR is accepted, this repo will be archived. Otherwise, if rejected, it will remain. It exists here to be easily installable during the interim. 

# HyprExpo+
HyprExpo+ is a fork of [HyprExpo](https://github.com/hyprwm/hyprland-plugins/tree/main/hyprexpo) that adds additional functionality.

https://github.com/user-attachments/assets/861baa26-46b6-4fa8-8d37-65cbb9ecbed4

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
        border_width = 2
        # Border colors support both solid (rgb/hex) and gradient formats
        # Solid: rgb(66ccff) or 0xFF66CCFF
        # Gradient: rgba(33ccffee) rgba(00ff99ee) 45deg
        border_color_current = rgb(66ccff)
        border_color_focus = rgb(ffcc66)
        border_color_hover = rgb(aabbcc)
        # rounded corners for workspace tiles
        tile_rounding = 0            # px
        tile_rounding_power = 2.0
        # optional state overrides
        tile_rounding_focus = -1      # -1 = inherit
        tile_rounding_current = -1
        tile_rounding_hover = -1

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

## Configuration

### Layout & Display

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:columns` | int | how many desktops per row | `3` |
| `plugin:hyprexpo:gaps_in` | int | inner spacing between tiles (px) | `5` |
| `plugin:hyprexpo:gaps_out` | int | outer margin around grid (px), animated during open/close | `0` |
| `plugin:hyprexpo:bg_col` | color | grid background color | `rgb(111111)` |
| `plugin:hyprexpo:workspace_method` | string | placement: `center current` or `first <ws>` | `center current` |
| `plugin:hyprexpo:skip_empty` | bool (int) | skip empty workspaces using selector `m` (`1`) or show all with `r` (`0`) | `0` |
| `plugin:hyprexpo:gesture_distance` | int | swipe distance considered "max" (px) | `200` |

### Tile Appearance

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:tile_rounding` | int | corner radius (px) | `0` |
| `plugin:hyprexpo:tile_rounding_power` | float | rounding power (curve exponent) | `2.0` |
| `plugin:hyprexpo:tile_rounding_focus` | int | focus tile radius (`-1` = inherit) | `-1` |
| `plugin:hyprexpo:tile_rounding_current` | int | current tile radius (`-1` = inherit) | `-1` |
| `plugin:hyprexpo:tile_rounding_hover` | int | hover tile radius (`-1` = inherit) | `-1` |
| `plugin:hyprexpo:border_width` | int | border thickness (px) | `2` |
| `plugin:hyprexpo:border_color_current` | string | current tile border - solid: `rgb(66ccff)` or `0xFF66CCFF`, gradient: `rgba(33ccffee) rgba(00ff99ee) 45deg` | `rgb(66ccff)` |
| `plugin:hyprexpo:border_color_focus` | string | focused tile border - supports solid or gradient (see above) | `rgb(ffcc66)` |
| `plugin:hyprexpo:border_color_hover` | string | hovered tile border - supports solid or gradient (see above) | `rgb(aabbcc)` |
| `plugin:hyprexpo:border_grad_current` | string | **DEPRECATED** - use `border_color_current` with gradient format instead | empty |
| `plugin:hyprexpo:border_grad_focus` | string | **DEPRECATED** - use `border_color_focus` with gradient format instead | empty |
| `plugin:hyprexpo:border_grad_hover` | string | **DEPRECATED** - use `border_color_hover` with gradient format instead | empty |
| `plugin:hyprexpo:border_style` | string | **DEPRECATED** - border style is now auto-detected from color format | `simple` |

### Workspace Labels

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:label_enable` | bool (int) | enable workspace labels | `1` |
| `plugin:hyprexpo:label_text_mode` | string | content: `token` (1,2..9,0,a-z), `index` (visual order), `id` (workspace ID) | `token` |
| `plugin:hyprexpo:label_token_map` | string | override up to 50 tokens (comma-sep), e.g. `1,2,3,,,6` (empty entries skip) | empty |
| `plugin:hyprexpo:label_position` | string | anchor: `top-left` \| `top-right` \| `bottom-left` \| `bottom-right` \| `center` | `center` |
| `plugin:hyprexpo:label_offset_x` | int | horizontal offset from anchor (px) | `0` |
| `plugin:hyprexpo:label_offset_y` | int | vertical offset from anchor (px) | `0` |
| `plugin:hyprexpo:label_show` | string | visibility: `always` \| `hover` \| `focus` \| `hover+focus` \| `current+focus` \| `never` | `always` |
| `plugin:hyprexpo:label_color_default` | color | default state text color | `rgb(ffffff)` |
| `plugin:hyprexpo:label_color_hover` | color | hover state text color | `rgb(eeeeee)` |
| `plugin:hyprexpo:label_color_focus` | color | focus state text color | `rgb(ffcc66)` |
| `plugin:hyprexpo:label_color_current` | color | current workspace text color | `rgb(66ccff)` |
| `plugin:hyprexpo:label_scale_hover` | float | scale multiplier on hover | `1.0` |
| `plugin:hyprexpo:label_scale_focus` | float | scale multiplier on focus | `1.0` |
| `plugin:hyprexpo:label_font_size` | int | base font size (px) | `16` |
| `plugin:hyprexpo:label_font_family` | string | Pango font family | `Sans` |
| `plugin:hyprexpo:label_font_bold` | bool (int) | bold text | `0` |
| `plugin:hyprexpo:label_font_italic` | bool (int) | italic text | `0` |
| `plugin:hyprexpo:label_text_underline` | bool (int) | underline text | `0` |
| `plugin:hyprexpo:label_text_strikethrough` | bool (int) | strikethrough text | `0` |
| `plugin:hyprexpo:label_pixel_snap` | bool (int) | snap positions to integer pixels for sharper text | `1` |
| `plugin:hyprexpo:label_center_adjust_x` | int | manual center nudge X (px) | `0` |
| `plugin:hyprexpo:label_center_adjust_y` | int | manual center nudge Y (px) | `0` |
| `plugin:hyprexpo:label_bg_enable` | bool (int) | draw background bubble behind text | `1` |
| `plugin:hyprexpo:label_bg_shape` | string | bubble shape: `circle` \| `square` \| `rounded` | `circle` |
| `plugin:hyprexpo:label_bg_rounding` | int | radius for `rounded` shape (px) | `8` |
| `plugin:hyprexpo:label_bg_color` | color | bubble background color | `rgba(00000088)` |
| `plugin:hyprexpo:label_padding` | int | bubble padding around text (px) | `8` |

### Keyboard Navigation

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:keynav_enable` | bool (int) | enable keyboard navigation + auto submap | `1` |
| `plugin:hyprexpo:keynav_wrap_h` | bool (int) | wrap horizontally at row edges | `1` |
| `plugin:hyprexpo:keynav_wrap_v` | bool (int) | wrap vertically at column edges | `1` |
| `plugin:hyprexpo:keynav_reading_order` | bool (int) | row-major horizontal moves (wrap at ends if both h+v wrap enabled) | `0` |

### Scrolling Overview (Layout-Specific)

When using Hyprland's scrolling layout, HyprExpo automatically switches to a vertical scrolling overview mode.

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:scrolling:scroll_moves_up_down` | bool (int) | if enabled, mouse wheel scrolls through workspaces; if disabled, mouse wheel zooms | `1` |
| `plugin:hyprexpo:scrolling:default_zoom` | float | default zoom level on overview open (0.1 - 0.9) | `0.5` |

**Note:** Scrolling overview mode does not support keyboard navigation. Mouse/trackpad interaction is the primary input method for this layout.

## Usage

### Dispatchers

**Main expo dispatcher:**

```bash
bind = SUPER, g, hyprexpo:expo, <option>
```

| option | description |
| --- | --- |
| `toggle` | show overview if hidden, hide if shown |
| `on` / `enable` | show overview |
| `off` / `disable` | hide overview |
| `select` | select the hovered workspace |

**Keyboard navigation dispatchers** (active during overview):

| dispatcher | argument | description |
| --- | --- | --- |
| `hyprexpo:kb_focus` | `left` \| `right` \| `up` \| `down` | move focus across tiles |
| `hyprexpo:kb_confirm` | none | select the focused tile |
| `hyprexpo:kb_selecti` | `<index>` | select by 1-based visual index (1-46) |
| `hyprexpo:kb_selectn` | `<id>` | select by workspace ID (legacy, 0→10) |
| `hyprexpo:kb_select` | `<token>` | select by single token (1-9, 0, a-z) |

### Example Bindings

```bash
# Toggle overview
bind = SUPER, g, hyprexpo:expo, toggle

# Keyboard navigation submap (optional)
submap = hyprexpo
    # Arrow key navigation
    bind = , left,  hyprexpo:kb_focus, left
    bind = , right, hyprexpo:kb_focus, right
    bind = , up,    hyprexpo:kb_focus, up
    bind = , down,  hyprexpo:kb_focus, down
    bind = , return, hyprexpo:kb_confirm

    # Direct selection via numbers (1-10)
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

    # Extended selection via SHIFT+numbers (11-20)
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

    # Alphabetic selection (21-46)
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

### Gestures

Custom gesture keyword for HyprExpo-specific gestures:

```ini
# Swipe up with 4 fingers to toggle overview
hyprexpo_gesture = swipe:4:up, hyprexpo:expo, toggle
```

Uses the same syntax as Hyprland's `gesture` keyword.

### Per-Monitor Workspace Method

You can configure different workspace methods for different monitors using the `workspace_method` custom keyword:

```ini
# Global default (inside plugin block)
plugin {
    hyprexpo {
        workspace_method = center current
    }
}

# Per-monitor overrides (outside plugin block, at top level)
workspace_method = DP-1 first 1
workspace_method = HDMI-A-1 center current
workspace_method = eDP-1 first 10
```

The global keyword format: `workspace_method = MONITOR_NAME <center|first> <workspace>`
