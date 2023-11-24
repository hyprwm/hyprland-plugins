# hyprland-plugins

This repo houses official plugins for Hyprland.

> [!IMPORTANT]
> hyprland-plugins follows hyprland-git and requires hyprland-git to work properly.
> If you want to use a versioned hyprland, you'll have to reset hyprland-plugins
> to a commit from before that hyprland version's release date.

# Plugin list
 - borders-plus-plus -> adds one or two additional borders to windows
 - csgo-vulkan-fix -> fixes custom resolutions on CS:GO with `-vulkan`
 - hyprbars -> adds title bars to windows
 - hyprtrails -> adds smooth trails behind moving windows
 - hyprwinwrap -> clone of xwinwrap, allows you to put any app as a wallpaper

# Nix

To use these plugins, it's recommended that you are already using the
[Hyprland flake](https://github.com/hyprwm/Hyprland).
First, add this flake to your inputs:

```nix
inputs = {
  # ...
  hyprland.url = "github:hyprwm/Hyprland";
  hyprland-plugins = {
    url = "github:hyprwm/hyprland-plugins";
    inputs.hyprland.follows = "hyprland";
  };

  # ...
};
```

The `inputs.hyprland.follows` guarantees the plugins will always be built using
your locked Hyprland version, thus you will never get version mismatches that
lead to errors.

After that's done, you can use the plugins with the Home Manager module like
this:

```nix
{inputs, pkgs, ...}: {
  wayland.windowManager.hyprland = {
    enable = true;
    # ...
    plugins = [
      inputs.hyprland-plugins.packages.${pkgs.system}.hyprbars
      # ...
    ];
  };
}
```

# Contributing

Feel free to open issues and MRs with fixes.

If you want your plugin added here, contact vaxry beforehand.
