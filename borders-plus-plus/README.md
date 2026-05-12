# borders-plus-plus

Allows you to add one or two additional borders to your windows.

The borders added are static.

Example Config:

```lua
hl.config({
    plugin = {
        borders_plus_plus = {
            add_borders = 1
            natural_rounding = true

            col = {
                border_1 = "rgb(ffffff)"
            }

            border_size_1 = 10
        }
    }
})
```