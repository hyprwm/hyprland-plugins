# csgo-vulkan-fix

Steam Overlay is unsupported.

If you want to play CS:GO with `-vulkan`, you're locked to your native res.

With this plugin, you aren't anymore.

CSGO launch options:
```
-window -w <RESX> -h <RESY> -vulkan
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
