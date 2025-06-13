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

`enabled` -> (bool) whether to enable the bars

`bar_color` -> (col) bar's background color

`bar_height` -> (int) bar's height (default `15`)

`bar_blur` -> (bool) whether to blur the bar. Also requires the global blur to be enabled.

`col.text` -> (col) bar's title text color

`bar_title_enabled` -> (bool) whether to render the title (default `true`)

`bar_text_size` -> (int) bar's title text font size (default `10`)

`bar_text_font` -> (str) bar's title text font (default `Sans`)

`bar_text_align` -> (str) bar's title text alignment (default `center`, can also be `left`)

`bar_buttons_alignment` -> (str) bar's buttons alignment (default: `right`, can also be `left`)

`bar_part_of_window` -> (bool) whether the bar is a part of the main window (if it is, stuff like shadows render around it)

`bar_precedence_over_border` -> (bool) whether the bar should have a higher priority than the border (border will be around the bar)

`bar_padding` -> (int) left / right edge padding (default `7`)

`bar_button_padding` -> (int) padding between the buttons (default `5`)

`icon_on_hover` -> (bool) whether the icons show on mouse hovering over the buttons (default `false`)

## Buttons Config

Use the `hyprbars-button` keyword.

```ini
hyprbars-button = bgcolor, size, icon, on-click, fgcolor
```

## Window rules

Hyprbars supports the following _dynamic_ window rules:

`plugin:hyprbars:nobar` -> disables the bar on matching windows.
`plugin:hyprbars:bar_color` -> sets the bar background color on matching windows.
`plugin:hyprbars:title_color` -> sets the bar title color on matching windows.
