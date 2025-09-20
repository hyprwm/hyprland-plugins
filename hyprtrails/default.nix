{
  lib,
  hyprland,
  hyprlandPlugins,
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "hyprtrails";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins/tree/main/hyprtrails";
    description = "Smooth trails behind moving windows for Hyprland";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
