# borders-plus-plus

Allows you to add one or two additional borders to your windows.

The borders added are static.

Example Config:
```
plugin {
    borders-plus-plus {
        add_borders = 1 # 0 - 9

        # you can add up to 9 borders
        col.border_1 = rgb(ffffff)
        col.border_2 = rgb(2222ff)

        # -1 means "default" as in the one defined in general:border_size
        border_size_1 = 10
        border_size_2 = -1

        # makes outer edges match rounding of the parent. Turn on / off to better understand. Default = on.
        natural_rounding = yes
    }
}
```