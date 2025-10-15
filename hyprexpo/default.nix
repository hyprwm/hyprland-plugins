{
  lib,
  hyprland,
  hyprlandPlugins,
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "hyprexpo-plus";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  meta = with lib; {
    homepage = "https://github.com/sandwichfarm/hyprexpo-plus";
    description = "An enhanced Hyprland workspaces overview plugin";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
