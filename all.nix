{
  builder,
  lib,
}:
lib.mapAttrs (n: v: builder v) {
  borders-plus-plus = {
    pname = "borders-plus-plus";
    version = "0.1";
    src = ./.;

    meta = with lib; {
      homepage = "https://github.com/hyprwm/hyprland-plugins";
      description = "Hyprland borders-plus-plus plugin";
      license = licenses.bsd3;
      platforms = platforms.linux;
    };
  };

  csgo-vulkan-fix = {
    pname = "csgo-vulkan-fix";
    version = "0.1";
    src = ./.;

    meta = with lib; {
      homepage = "https://github.com/hyprwm/hyprland-plugins";
      description = "Hyprland CS:GO Vulkan fix";
      license = licenses.bsd3;
      platforms = platforms.linux;
    };
  };

  hyprbars = {
    pname = "hyprbars";
    version = "0.1";
    src = ./.;

    meta = with lib; {
      homepage = "https://github.com/hyprwm/hyprland-plugins";
      description = "Hyprland window title plugin";
      license = licenses.bsd3;
      platforms = platforms.linux;
    };
  };
}
