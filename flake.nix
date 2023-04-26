{
  description = "Hyprland Plugins";

  inputs.hyprland.url = "github:hyprwm/Hyprland";

  outputs = {
    self,
    hyprland,
  }: let
    inherit (hyprland.inputs) nixpkgs;
    withPkgsFor = fn: nixpkgs.lib.genAttrs (builtins.attrNames hyprland.packages) (system: fn system nixpkgs.legacyPackages.${system});
  in {
    packages = withPkgsFor (system: pkgs: {
      borders-plus-plus = pkgs.callPackage ./borders-plus-plus {inherit (hyprland.packages.${system}) hyprland;};
      csgo-vulkan-fix = pkgs.callPackage ./csgo-vulkan-fix {inherit (hyprland.packages.${system}) hyprland;};
      hyprbars = pkgs.callPackage ./hyprbars {inherit (hyprland.packages.${system}) hyprland;};
    });

    devShells = withPkgsFor (system: pkgs: {
      default = pkgs.mkShell {
        name = "hyprland-plugins";
        buildInputs = [hyprland.packages.${system}.hyprland];
        inputsFrom = [hyprland.packages.${system}.hyprland];
      };
    });
  };
}
