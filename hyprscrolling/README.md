# hyprscrolling

Adds a scrolling layout to Hyprland.

**This plugin is a work in progress!**

## Config

*All config values are in `plugin:hyprscrolling`.*

| name | description | type | default |
| -- | -- | -- | -- |
| fullscreen_on_one_column | if there's only one column, should it be fullscreen | bool | false |
| column_width | default column width as a fraction of the monitor width | float [0 - 1] | 0.5 |
| explicit_column_widths | a comma-separated list of widths for columns to be used with `+conf` or `-conf` | string | `0.333, 0.5, 0.667, 1.0` |
| focus_fit_method | when a column is focused, what method to use to bring it into view. 0 - center, 1 - fit | int | 0 |
| center_on_focus | always keep the currently focused window in the center | bool | false |

## Layout messages

| name | description | params |
| --- | --- | --- |
| move | move the layout horizontally, by either a relative logical px (`-200`, `+200`) or columns (`+col`, `-col`) | move data |
| colresize | resize the current column, to either a value or by a relative value e.g. `0.5`, `+0.2`, `-0.2` or cycle the preconfigured ones with `+conf` or `-conf`. Can also be `all (number)` for resizing all columns to a specific width | relative float / relative conf |
| movewindowto | same as the movewindow dispatcher but supports promotion to the right at the end | direction |
| fit | executes a fit operation based on the argument. Available: `active`, `visible`, `all`, `toend`, `tobeg` | fit mode |
| focus | moves the focus and centers the layout, while also wrapping instead of moving to neighbring monitors. | direction |
| promote | moves a window to its own new column | none |
| swapcol | swap the current column with a neighbor. `l`/`r`| direction |

## With dispatchers
`hyprctl dispatch colresize +conf`

## With bindings
`bind = SUPER, left, focus, l`
