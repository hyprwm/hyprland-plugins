{
  lib,
  stdenv,
  hyprland,
}:
stdenv.mkDerivation {
  pname = "hyprwinwrap";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  buildInputs = [hyprland] ++ hyprland.buildInputs;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins";
    description = "Hyprland version of xwinwrap";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
