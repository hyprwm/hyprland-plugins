# hyprbars

Adds simple title bars to windows.

![image](https://github.com/user-attachments/assets/184a66b9-eb91-4f6f-8953-b265a2735939)

_Example config taken from [EvanKoe's dotfiles](https://github.com/EvanKoe/dotfiles_2023) (requires light wallpaper to look good):_  

```
plugin {
	hyprbars {
		bar_height = 30
		bar_buttons_alignment = left
		bar_part_of_window = true
		bar_blur = true
		bar_padding = 12
		bar_button_padding = 8
		bar_color = rgba(ffffffcc)
		col.text = rgb(212121)

		hyprbars-button = rgb(dd0000), 15, , hyprctl dispatch killactive
		hyprbars-button = rgb(00dd00), 15, , hyprctl dispatch fullscreen 1
		hyprbars-button = rgb(eeee00), 15, , hyprctl dispatch pseudo
		hyprbars-button = rgb(5577ff), 15, , hyprctl dispatch togglefloating
	}
}
windowrule = nofocus,class:^$,title:^$,xwayland:1,floating:1,fullscreen:0,pinned:0
windowrule = plugin:hyprbars:bar_color rgb(CDE7ED), class:^(google-chrome)

windowrule = plugin:hyprbars:bar_color rgba(1d2330ea), class:^(kitty)
windowrule = plugin:hyprbars:title_color rgb(dddddd), class:^(kitty)

windowrule = plugin:hyprbars:bar_color rgb(273F44), class:^(google-chrome) initialTitle:DevTools
windowrule = plugin:hyprbars:title_color rgb(dddddd), class:^(google-chrome) initialTitle:DevTools
```

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

        # cmd to run on double click of the bar
        on_double_click = hyprctl dispatch fullscreen 1
    }
}
```

| property | type | description | default |
| --- | --- | --- | --- |
`enabled` | bool | whether to enable the bars |
`bar_color` | color | bar's background color
`bar_height` | int | bar's height | `15`
`bar_blur` | bool | whether to blur the bar. Also requires the global blur to be enabled.
`col.text` | color | bar's title text color
`bar_title_enabled` | bool | whether to render the title | `true`
`bar_text_size` | int | bar's title text font size | `10`
`bar_text_font` | str | bar's title text font | `Sans`
`bar_text_align` | left, center | bar's title text alignment | `center`
`bar_buttons_alignment` | right, left | bar's buttons alignment | `right`
`bar_part_of_window` | bool | whether the bar is a part of the main window (if it is, stuff like shadows render around it)
`bar_precedence_over_border` | bool | whether the bar should have a higher priority than the border (border will be around the bar)
`bar_padding` | int | left / right edge padding | `7`
`bar_button_padding` | int | padding between the buttons | `5`
`icon_on_hover` | bool | whether the icons show on mouse hovering over the buttons | `false`
`inactive_button_color` | col | buttons bg color when window isn't focused
`on_double_click` | str | command to run on double click of the bar (not on a button)

## Buttons Config

Use the `hyprbars-button` keyword.

```ini
hyprbars-button = bgcolor, size, icon, on-click, fgcolor
```

## Window rules

Hyprbars supports the following _dynamic_ [window rules](https://wiki.hypr.land/Configuring/Window-Rules/):

`plugin:hyprbars:nobar` -> disables the bar on matching windows.  
`plugin:hyprbars:bar_color` -> sets the bar background color on matching windows.  
`plugin:hyprbars:title_color` -> sets the bar title color on matching windows.  

Example:
```bash
# Sets the bar color in red for all windows that have 'myClass' as a class
windowrule = plugin:hyprbars:bar_color rgb(ff0000), class:^(myClass)
```
