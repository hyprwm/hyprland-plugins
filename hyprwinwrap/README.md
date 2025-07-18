# hyprwinwrap

Clone of xwinwrap for hyprland.

Example config:
```ini
plugin {
    hyprwinwrap {
        # class is an EXACT match and NOT a regex!
        class = kitty-bg
        # you can also use title
        title = kitty-bg
        # you can add the position of the window in a percentage
        pos_x = 25
        pos_y = 30
        # you can add the size of the window in a percentage
        size_x = 40
        size_y = 70
    }
}

```

Launch `kitty -c "~/.config/hypr/kittyconfigbg.conf" --class="kitty-bg" "/home/vaxry/.config/hypr/cava.sh"`

Example script for cava:

```sh
#!/bin/sh
sleep 1 && cava
```

_sleep required because resizing happens a few ms after open, which breaks cava_
