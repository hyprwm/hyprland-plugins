{
  lib,
  stdenv,
  hyprland,
}:
stdenv.mkDerivation {
  name = "borders-plus-plus";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  buildInputs = [hyprland] ++ hyprland.buildInputs;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins";
    description = "Hyprland borders-plus-plus plugin";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
