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

| name | description | type | default |
| --- | --- | --- | --- |
| enable | enable or disable the plugin | bool | true |
| animate_floating | animate floating windows | bool | true |
| keyboard_focus_animation | animation for keyboard-driven focus changes (`flash` / `shrink` / `slide` / `none`) | str | flash |
| mouse_focus_animation | animation for mouse-driven focus changes (`flash` / `shrink` / `slide` / `none`) | str | none |
| fade_opacity | for flash, opacity at the peak of the animation | float | 0.8 |
| shrink_percentage | for shrink, fraction of the window size to scale to at the peak | float | 0.95 |
| slide_height | for slide, how many pixels to slide upward | float | 20 |
