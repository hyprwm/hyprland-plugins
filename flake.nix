{
  inputs.hyprland.url = "github:hyprwm/Hyprland";

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
        wlroots = hyprland.packages.${system}.wlroots-hyprland;
      };
    });
  };
}
