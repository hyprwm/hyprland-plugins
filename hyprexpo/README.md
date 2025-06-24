# hyprexpo

Overview plugin like gnome kde or wf.

## Config
A great start to configure this plugin would be this code:  
```ini
plugin {
    hyprexpo {
        columns = 3
        gap_size = 5
        bg_col = rgb(111111)
        workspace_method = center current # [center/first] [workspace] e.g. first 1 or center m+1

        enable_gesture = true # laptop touchpad
        gesture_fingers = 3  # 3 or 4
        gesture_distance = 300 # how far is the "max"
        gesture_positive = true # positive = swipe down. Negative = swipe up.
    }
}
```

### Properties

| property | type | description | default |
| --- | --- | --- | --- |
columns | number | how many desktops are displayed on one line | `3`
gap_size | number | gap between desktops | `5`
bg_col | color | color in gaps (between desktops) | `rgb(000000)`
workspace_method | [center/first] [workspace] | position of the desktops | `center current`
enable_gesture | boolean | enable touchp[ad gestures | `true`
gesture_fingers | `3` or `4` | how many fingers are needed in the gesture | `3`
gesture_distance | number | how far is the max | `300`
gesture_positive | boolean | whether to swipe down (true), or up (false) | `true`

### Binding
```bash
# hyprland.conf
bind = SUPER, grave, hyprexpo:expo, OPTION
```
Here are a list of options you can use:  
| option | description |
| --- | --- |
toggle | displays if hidden, hide if displayed
select | selects the hovered desktop
off | hides the overview
disable | same as `off`
on | displays the overview
enable | same as `on`

