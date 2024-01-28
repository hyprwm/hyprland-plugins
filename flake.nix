{
  description = "Hyprland Plugins";

  inputs.hyprland.url = "github:hyprwm/Hyprland";

  outputs = inputs: let
    inherit (inputs.hyprland.inputs) nixpkgs;
    withPkgsFor = fn: nixpkgs.lib.genAttrs (builtins.attrNames inputs.hyprland.packages) (system: fn system nixpkgs.legacyPackages.${system});
  in {
    packages = withPkgsFor (system: pkgs: let
      hyprland = inputs.hyprland.packages.${system}.hyprland;
      buildPlugin = path: extraArgs:
        pkgs.callPackage path {
          inherit hyprland;
          inherit (hyprland) stdenv;
        }
        // extraArgs;
    in {
      borders-plus-plus = buildPlugin ./borders-plus-plus {};
      csgo-vulkan-fix = buildPlugin ./csgo-vulkan-fix {};
      hyprbars = buildPlugin ./hyprbars {};
      hyprtrails = buildPlugin ./hyprtrails {};
      hyprwinwrap = buildPlugin ./hyprwinwrap {};
    });

    checks = withPkgsFor (system: pkgs: inputs.self.packages.${system});

    devShells = withPkgsFor (system: pkgs: {
      default = pkgs.mkShell.override {stdenv = pkgs.gcc13Stdenv;} {
        name = "hyprland-plugins";
        buildInputs = [inputs.hyprland.packages.${system}.hyprland];
        inputsFrom = [inputs.hyprland.packages.${system}.hyprland];
      };
    });
  };
}
