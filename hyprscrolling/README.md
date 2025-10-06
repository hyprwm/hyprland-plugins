# hyprscrolling

Adds a scrolling layout to Hyprland.

**This plugin is a work in progress!**

## Config

Make sure to set the `general:layout` to `scrolling` to use this layout.

All config values can be set in your Hyprland config file, for example:
```
plugin {
    hyprscrolling {
        column_width = 0.7
        fullscreen_on_one_column = false
    }
}
```

| name | description | type | default |
| -- | -- | -- | -- |
| fullscreen_on_one_column | if there's only one column, should it be fullscreen | bool | false |
| column_width | default column width as a fraction of the monitor width | float [0 - 1] | 0.5 |
| explicit_column_widths | a comma-separated list of widths for columns to be used with `+conf` or `-conf` | string | `0.333, 0.5, 0.667, 1.0` |
| focus_fit_method | when a column is focused, what method to use to bring it into view. 0 - center, 1 - fit | int | 0 |
| follow_focus | when a window is focused, the layout will move to make it visible | bool | true |


## Layout messages

| name | description | params |
| --- | --- | --- |
| move | move the layout horizontally, by either a relative logical px (`-200`, `+200`) or columns (`+col`, `-col`) | move data |
| colresize | resize the current column, to either a value or by a relative value e.g. `0.5`, `+0.2`, `-0.2` or cycle the preconfigured ones with `+conf` or `-conf`. Can also be `all (number)` for resizing all columns to a specific width | relative float / relative conf |
| movewindowto | same as the movewindow dispatcher but supports promotion to the right at the end | direction |
| fit | executes a fit operation based on the argument. Available: `active`, `visible`, `all`, `toend`, `tobeg` | fit mode |
| focus | moves the focus and centers the layout, while also wrapping instead of moving to neighbring monitors. | direction |
| promote | moves a window to its own new column | none |
| swapcol | Swaps the current column with its neighbor to the left (`l`) or right (`r`). The swap wraps around (e.g., swapping the first column left moves it to the end). | `l` or `r` |
| movecoltoworkspace | Moves the entire current column to the specified workspace, preserving its internal layout. Works with existing, new, and special workspaces. e.g. like `1`, `2`, `-1`, `+2`, `special`, etc. | workspace identifier|
| togglefit | Toggle the focus_fit_method (center, fit) | none |

Example key bindings for your Hyprland config:
```
bind = $mainMod, period, layoutmsg, move +col
bind = $mainMod, comma, layoutmsg, move -col
bind = $mainMod SHIFT, period, layoutmsg, movewindowto r
bind = $mainMod SHIFT, comma, layoutmsg, movewindowto l
bind = $mainMod SHIFT, up, layoutmsg, movewindowto u
bind = $mainMod SHIFT, down, layoutmsg, movewindowto d
```
