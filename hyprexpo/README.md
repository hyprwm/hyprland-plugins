# hyprexpo

Overview plugin like gnome kde or wf.

## Config

```ini
# example
bind = SUPER,tab,hyprexpo:expo

plugin {
    hyprexpo {
        columns = 3
        gap_size = 5
        bg_col = rgb(111111)
        workspace_method = center current # [center/first] [workspace] e.g. first 1 or center m+1
        enable_gesture = true # laptop touchpad, 4 fingers
    }
}

```