# hyprbars

Adds simple title bars to windows.

![preview](https://i.ibb.co/GkDTL4Q/20230228-23h20m36s-grim.png)

## Config

All config options are in `plugin:hyprbars`:

```
plugin {
    hyprbars {
        # config
        buttons {
            # button config
        }
    }
}
```

`bar_color` -> (col) bar's background color

`bar_height` -> (int) bar's height (default 15)

`col.text` -> (col) bar's title text color

`bar_text_size` -> (int) bar's title text font size (default 10)

`bar_text_font` -> (str) bar's title text font (default "Sans")

## Buttons Config

`button_size` -> (int) the size of the buttons.

`col.maximize` -> (col) maximize button color

`col.close` -> (col) close button color
