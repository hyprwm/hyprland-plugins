# csgo-vulkan-fix

Originally meant for csgo / cs2, but can work with any app, really.

csgo-vulkan-fix is a way to force apps to a fake resolution without
them realizing it.

If you want to play CS2, you're locked to your native res.
Other resolutions (especially not 16:9) are wonky.

With this plugin, you aren't anymore.

This is also useful for when you are scaling and want to force any game to a custom resolution.

CS2 launch options:
```
-vulkan -window -w <RESX> -h <RESY> -vulkan
```

example plugin config:
```
plugin {
    csgo-vulkan-fix {
        # Whether to fix the mouse position. A select few apps might be wonky with this.
        fix_mouse = true

        # Add apps with vkfix-app = initialClass, width, height
        vkfix-app = cs2, 1650, 1050
        vkfix-app = myapp, 1920, 1080
    }
}
```

fullscreen the game manually and enjoy.