{
  lib,
  hyprland,
  hyprlandPlugins,
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "hyprfocus";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins/tree/main/hyprfocus";
    description = "Hyprland flashfocus plugin";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
