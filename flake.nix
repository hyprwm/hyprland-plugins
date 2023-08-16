{
  description = "Hyprland Plugins";

  inputs.hyprland.url = "github:hyprwm/Hyprland";

  outputs = {
    self,
    hyprland,
  }: let
    inherit (hyprland.inputs) nixpkgs systems;
    inherit (nixpkgs) lib;
    inherit (import ./lib) pluginBuilder;
    withPkgsFor = fn: lib.genAttrs (import systems) (system: fn system nixpkgs.legacyPackages.${system});
  in {
    packages = withPkgsFor (system: pkgs: let
      builder = pluginBuilder hyprland pkgs.gcc13Stdenv;
    in {
      borders-plus-plus = builder (import ./borders-plus-plus lib);
      csgo-vulkan-fix = builder (import ./csgo-vulkan-fix lib);
      hyprbars = builder (import ./hyprbars lib);
    });

    legacyPackages = withPkgsFor (system: pkgs: let
      builder = pluginBuilder hyprland pkgs.stdenv;
    in {
      all = import ./all.nix {inherit builder lib;};
    });

    devShells = withPkgsFor (system: pkgs: {
      default = pkgs.mkShell.override {stdenv = pkgs.gcc13Stdenv;} {
        name = "hyprland-plugins";
        buildInputs = [hyprland.packages.${system}.hyprland];
        inputsFrom = [hyprland.packages.${system}.hyprland];
      };
    });
  };
}
