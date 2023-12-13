# hyprbars

Adds simple title bars to windows.

![preview](https://i.ibb.co/hLDRCpT/20231029-22h30m05s.png)

## Config

All config options are in `plugin:hyprbars`:

```
plugin {
    hyprbars {
        # example config
        bar_height = 20

        # example buttons (R -> L)
        # hyprbars-button = color, size, on-click
        hyprbars-button = rgb(ff4040), 10, 󰖭, hyprctl dispatch killactive
        hyprbars-button = rgb(eeee11), 10, , hyprctl dispatch fullscreen 1
    }
}
```

`bar_color` -> (col) bar's background color

`bar_height` -> (int) bar's height (default `15`)

`col.text` -> (col) bar's title text color

`bar_text_size` -> (int) bar's title text font size (default `10`)

`bar_text_font` -> (str) bar's title text font (default `Sans`)

`bar_text_align` -> (str) bar's title text alignment (default `center`, can also be `left`)

`bar_buttons_alignment` -> (str) bar's buttons alignment (default: `right`, can also be `left`)

`bar_part_of_window` -> (bool) whether the bar is a part of the main window (if it is, stuff like shadows render around it)

`bar_precedence_over_border` -> (bool) whether the bar should have a higher priority than the border (border will be around the bar)

## Buttons Config

Use the `hyprbars-button` keyword.

```ini
hyprbars-button = color, size, icon, on-click
```