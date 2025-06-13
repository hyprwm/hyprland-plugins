# hyprwinwrap

Clone of xwinwrap for hyprland.

Example config:
```ini
plugin {
    hyprwinwrap {
        # class is an EXACT match and NOT a regex!
        class = kitty-bg
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