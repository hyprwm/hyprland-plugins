# hyprfocus

Flashfocus for Hyprland.

## Configuring

### Animations

Hyprfocus exposes two animation leaves: `hyprfocusIn` and `hyprfocusOut`:
```ini
animation = hyprfocusIn, 1, 1.7, myCurve
animation = hyprfocusOut, 1, 1.7, myCurve2
```

### Variables

| name                     | description                                                          | type    | default |
|--------------------------|----------------------------------------------------------------------|---------|---------|
| `mode`                   | which mode to use (flash / bounce / slide)                           | str     | flash   |
| `only_on_monitor_change` | whether to only perform the animation when changing between monitors | boolean | false   |
| `fade_opacity`           | for flash, what opacity to flash to                                  | float   | 0.8     |
| `bounce_strength`        | for bounce, what fraction of the window's size to jump to            | float   | 0.95    |
| `slide_height`           | for slide, how far up to slide                                       | float   | 20      |
