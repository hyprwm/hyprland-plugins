# xtra-dispatchers

Adds some additional dispatchers to Hyprland.

## Dispatchers

| name | description | params |
| -- | -- | -- |
| moveorexec | moves window to the current workspace, or executes if it's not found. `WINDOW` cannot contain commas | `WINDOW,CMD` |
| throwunfocused | throws all unfocused windows on the current workspace to the given workspace | `WORKSPACE` |
| bringallfrom | kinda inverse of throwunfocused. Bring all windows from a given workspace to the current one. | `WORKSPACE` |