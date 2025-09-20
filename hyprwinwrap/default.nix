{
  lib,
  hyprland,
  hyprlandPlugins,
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "hyprwinwrap";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins/tree/main/hyprwinwrap";
    description = "Hyprland version of xwinwrap";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
