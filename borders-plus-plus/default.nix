{
  lib,
  hyprland,
  hyprlandPlugins,
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "borders-plus-plus";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins/tree/main/borders-plus-plus";
    description = "Hyprland borders-plus-plus plugin";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
