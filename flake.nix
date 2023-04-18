{
  inputs.hyprland.url = "github:horriblename/Hyprland/nix-pluginenv";

  outputs = {
    self,
    hyprland,
  }: let
    inherit (hyprland.inputs) nixpkgs;
    withPkgsFor = fn: nixpkgs.lib.genAttrs (builtins.attrNames hyprland.packages) (system: fn system nixpkgs.legacyPackages.${system});
  in {
    packages = withPkgsFor (system: pkgs: {
      hyprbars = pkgs.callPackage ./hyprbars {
        inherit (hyprland.packages.${system}) hyprland;
      };
    });
  };
}
