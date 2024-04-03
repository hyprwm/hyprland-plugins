# hyprland-plugins

This repo houses official plugins for Hyprland.

> [!IMPORTANT]
> hyprland-plugins only officially supports installation via `hyprpm`.
> hyprland-plugins follows hyprland-git and requires you to be on hyprland-git
> or tagged >= v0.33.1.

# Plugin list
 - borders-plus-plus -> adds one or two additional borders to windows
 - csgo-vulkan-fix -> fixes custom resolutions on CS:GO with `-vulkan`
 - hyprbars -> adds title bars to windows
 - hyprexpo -> adds an expo-like workspace overview
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
