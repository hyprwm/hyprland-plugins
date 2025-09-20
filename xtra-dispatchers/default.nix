{
  lib,
  hyprland,
  hyprlandPlugins,
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "xtra-dispatchers";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins/tree/main/xtra-dispatchers";
    description = "Hyprland extra dispatchers plugin";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
