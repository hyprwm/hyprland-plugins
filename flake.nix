{
  description = "Official Hyprland Plugins";

  inputs = {
    hyprland.url = "github:hyprwm/Hyprland";
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
          self.overlays.gcc14Stdenv
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
        hyprlandPlugins =
          (prev.hyprlandPlugins
            or {})
          // {
            borders-plus-plus = callPackage ./borders-plus-plus {};
            csgo-vulkan-fix = callPackage ./csgo-vulkan-fix {};
            hyprbars = callPackage ./hyprbars {};
            hyprexpo = callPackage ./hyprexpo {};
            hyprtrails = callPackage ./hyprtrails {};
            hyprwinwrap = callPackage ./hyprwinwrap {};
          };
      };

      # TODO: remove when https://github.com/NixOS/nixpkgs/pull/365776 lands in master
      gcc14Stdenv = final: prev: {
        hyprlandPlugins =
          (prev.hyprlandPlugins or {})
          // {
            mkHyprlandPlugin = hyprland: args @ {pluginName, ...}:
              hyprland.stdenv.mkDerivation (
                args
                // {
                  pname = pluginName;
                  nativeBuildInputs = [prev.pkg-config] ++ args.nativeBuildInputs or [];
                  buildInputs = [hyprland] ++ hyprland.buildInputs ++ (args.buildInputs or []);
                  meta =
                    args.meta
                    // {
                      description = args.meta.description or "";
                      longDescription =
                        (args.meta.longDescription or "")
                        + "\n\nPlugins can be installed via a plugin entry in the Hyprland NixOS or Home Manager options.";
                    };
                }
              );
          };
      };
    };

    checks = eachSystem (system: self.packages.${system});

    devShells = eachSystem (system:
      with pkgsFor.${system}; {
        default = mkShell.override {stdenv = gcc14Stdenv;} {
          name = "hyprland-plugins";
          buildInputs = [hyprland.packages.${system}.hyprland];
          inputsFrom = [hyprland.packages.${system}.hyprland];
        };
      });
  };
}
