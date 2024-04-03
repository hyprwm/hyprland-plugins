# hyprexpo

Overview plugin like gnome kde or wf.

## Config

```ini

plugin {
    hyprexpo {
        columns = 3
        gap_size = 5
        bg_col = rgb(111111)
        workspace_method = center current # [center/first] [workspace] e.g. first 1 or center m+1

        enable_gesture = true # laptop touchpad, 4 fingers
        gesture_distance = 300 # how far is the "max"
        gesture_positive = true # positive = swipe down. Negative = swipe up.
    }
}

```