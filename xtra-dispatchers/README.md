# xtra-dispatchers

Adds some additional dispatchers to Hyprland.

## Dispatchers

All dispatchers here are called `plugin:xtd:name` e.g. `plugin:xtd:moveorexec`.

| name | description | params |
| -- | -- | -- |
| moveorexec | moves window to the current workspace, or executes if it's not found. `WINDOW` cannot contain commas | `WINDOW,CMD` |
| throwunfocused | throws all unfocused windows on the current workspace to the given workspace | `WORKSPACE` |
| bringallfrom | kinda inverse of throwunfocused. Bring all windows from a given workspace to the current one. | `WORKSPACE` |
| closeunfocused | close all unfocused windows on the current workspace. | none |
