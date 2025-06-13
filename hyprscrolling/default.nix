{
  lib,
  hyprland,
  hyprlandPlugins,
}:
hyprlandPlugins.mkHyprlandPlugin hyprland {
  pluginName = "hyprscrolling";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins";
    description = "Hyprland scrolling layout plugin";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
