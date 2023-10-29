# hyprbars

Adds simple title bars to windows.

![preview](https://i.ibb.co/GkDTL4Q/20230228-23h20m36s-grim.png)

## Config

All config options are in `plugin:hyprbars`:

```
plugin {
    hyprbars {
        # example config
        bar_height = 20

        # example buttons (R -> L)
        # hyprbars-button = color, size, on-click
        hyprbars-button = rgb(ff4040), 10, hyprctl dispatch killactive
        hyprbars-button = rgb(eeee11), 10, hyprctl dispatch fullscreen 1
    }
}
```

`bar_color` -> (col) bar's background color

`bar_height` -> (int) bar's height (default 15)

`col.text` -> (col) bar's title text color

`bar_text_size` -> (int) bar's title text font size (default 10)

`bar_text_font` -> (str) bar's title text font (default "Sans")

## Buttons Config

Use the `hyprbars-button` keyword.

```ini
hyprbars-button = color, size, on-click
```