{
  lib,
  hyprland,
  hyprlandPlugins,
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "csgo-vulkan-fix";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins/tree/main/csgo-vulkan-fix";
    description = "Hyprland CS:GO Vulkan fix";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
