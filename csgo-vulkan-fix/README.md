# csgo-vulkan-fix

Originally meant for csgo / cs2, but can work with any app, really.

csgo-vulkan-fix is a way to force apps to a fake resolution without
them realizing it.

If you want to play CS2, you're locked to your native res.
Other resolutions (especially not 16:9) are wonky.

With this plugin, you aren't anymore.

CS2 launch options:
```
-window -w <RESX> -h <RESY>
```

example plugin config:
```
plugin {
    csgo-vulkan-fix {
        res_w = 1680
        res_h = 1050

        # NOT a regex! This is a string and has to exactly match initial_title
        title = Counter-Strike 2

        # Whether to fix the mouse position. A select few apps might be wonky with this.
        fix_mouse = true
    }
}
```

fullscreen the game manually and enjoy.
