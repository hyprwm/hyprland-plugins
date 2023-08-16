hyprland: stdenv: {
  pname,
  version ? "dirty",
  src,
  buildInputs ? [],
  meta ? {},
}: let
  hl = hyprland.packages.${stdenv.hostPlatform.system}.default;
in
  stdenv.mkDerivation {
    inherit pname version src;

    inherit (hl) nativeBuildInputs;
    buildInputs = hl.buildInputs ++ [hl.dev] ++ buildInputs;

    inherit meta;
  }
