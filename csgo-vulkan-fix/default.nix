{
  lib,
  stdenv,
  hyprland,
}:
stdenv.mkDerivation {
  name = "csgo-vulkan-fix";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  buildInputs = [hyprland] ++ hyprland.buildInputs;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins";
    description = "Hyprland CS:GO Vulkan fix";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
