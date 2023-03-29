{
  stdenv,
  hyprland,
  pkg-config,
  pixman,
  libdrm,
  wlroots,
}:
stdenv.mkDerivation {
  name = "hyprbars";
  src = ./.;
  preConfigure = "rm Makefile";

  nativeBuildInputs = [
    pkg-config
  ];

  buildInputs = [
    pixman
    libdrm
    wlroots
  ];

  buildPhase = ''
    trap "set +x" err
    set -xeu

    $CXX --no-gnu-unique -shared \
      -std=c++23 \
      $(pkg-config --cflags pixman-1) \
      $(pkg-config --cflags libdrm) \
      $(pkg-config --cflags wlroots) \
      -I${hyprland.src}/ \
      main.cpp barDeco.cpp \
      -o $out

    set +x
  '';
}
