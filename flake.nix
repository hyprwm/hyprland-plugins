{
  description = "Hyprland Plugins";

  inputs = {
    hyprland.url = "github:hyprwm/Hyprland";
    systems.follows = "hyprland/systems";
  };

  outputs = {
    self,
    hyprland,
    systems,
    ...
  }: let
    inherit (hyprland.inputs) nixpkgs;
    inherit (nixpkgs) lib;
    eachSystem = lib.genAttrs (import systems);

    pkgsFor = eachSystem (system:
      import nixpkgs {
        localSystem.system = system;
        overlays = with self.overlays; [hyprland-plugins];
      });
  in {
    packages = eachSystem (system: {
      inherit (pkgsFor.${system}) borders-plus-plus csgo-vulkan-fix hyprbars hyprexpo hyprtrails hyprwinwrap;
    });

    overlays = {
      default = self.overlays.hyprland-plugins;
      mkHyprlandPlugin = lib.composeManyExtensions [
        hyprland.overlays.default
        (final: prev: {
          hyprlandPlugins =
            prev.hyprlandPlugins
            // {
              mkHyprlandPlugin = prev.hyprlandPlugins.mkHyprlandPlugin final.hyprland;
            };
        })
      ];
      hyprland-plugins = lib.composeManyExtensions [
        self.overlays.mkHyprlandPlugin
        (final: prev: let
          inherit (final) callPackage;
        in {
          borders-plus-plus = callPackage ./borders-plus-plus {};
          csgo-vulkan-fix = callPackage ./csgo-vulkan-fix {};
          hyprbars = callPackage ./hyprbars {};
          hyprexpo = callPackage ./hyprexpo {};
          hyprtrails = callPackage ./hyprtrails {};
          hyprwinwrap = callPackage ./hyprwinwrap {};
        })
      ];
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
