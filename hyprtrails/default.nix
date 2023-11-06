{
  lib,
  stdenv,
  hyprland,
}:
stdenv.mkDerivation {
  pname = "hyprtrails";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  buildInputs = [hyprland] ++ hyprland.buildInputs;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins";
    description = "Smooth trails behind moving windows for Hyprland";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
