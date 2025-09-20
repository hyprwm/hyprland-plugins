{
  lib,
  hyprland,
  hyprlandPlugins,
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "hyprbars";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins/tree/main/hyprbars";
    description = "Hyprland window title plugin";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
