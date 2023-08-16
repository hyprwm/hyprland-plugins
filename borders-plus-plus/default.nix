lib: {
  pname = "borders-plus-plus";
  version = "0.1";
  src = ./.;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins";
    description = "Hyprland borders-plus-plus plugin";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
