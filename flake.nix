{
  description = "Official Hyprland Plugins";

  inputs = {
    hyprland.url = "git+https://github.com/hyprwm/Hyprland?submodules=1";
    nixpkgs.follows = "hyprland/nixpkgs";
    systems.follows = "hyprland/systems";
  };

  outputs = {
    self,
    hyprland,
    nixpkgs,
    systems,
    ...
  }: let
    inherit (nixpkgs) lib;
    eachSystem = lib.genAttrs (import systems);

    pkgsFor = eachSystem (system:
      import nixpkgs {
        localSystem.system = system;
        overlays = [
          self.overlays.hyprland-plugins
          hyprland.overlays.hyprland-packages
        ];
      });
  in {
    packages = eachSystem (system: {
      inherit
        (pkgsFor.${system}.hyprlandPlugins)
        borders-plus-plus
        csgo-vulkan-fix
        hyprbars
        hyprexpo
        hyprtrails
        hyprwinwrap
        ;
    });

    overlays = {
      default = self.overlays.hyprland-plugins;

      hyprland-plugins = final: prev: let
        inherit (final) callPackage;
      in {
        hyprlandPlugins = prev.hyprlandPlugins or {} // {
          borders-plus-plus = callPackage ./borders-plus-plus {};
          csgo-vulkan-fix = callPackage ./csgo-vulkan-fix {};
          hyprbars = callPackage ./hyprbars {};
          hyprexpo = callPackage ./hyprexpo {};
          hyprtrails = callPackage ./hyprtrails {};
          hyprwinwrap = callPackage ./hyprwinwrap {};
        };
      };
    };

    checks = eachSystem (system: self.packages.${system});

    devShells = eachSystem (system:
      with pkgsFor.${system}; {
        default = mkShell.override {stdenv = gcc13Stdenv;} {
          name = "hyprland-plugins";
          buildInputs = [hyprland.packages.${system}.hyprland];
          inputsFrom = [hyprland.packages.${system}.hyprland];
        };
      });
  };
}
