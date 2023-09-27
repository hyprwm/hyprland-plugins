# csgo-vulkan-fix

Updated for CS2.

If you want to play CS2, you're locked to your native res.
Other resolutions (especially not 16:9) are wonky.

With this plugin, you aren't anymore.

CS2 launch options:
```
-vulkan -window -w <RESX> -h <RESY> -vulkan
```

example plugin config:
```
plugin {
    csgo-vulkan-fix {
        res_w = 1680
        res_h = 1050
    }
}
```

fullscreen the game manually and enjoy.