{
  lib,
  hyprland,
  hyprlandPlugins,
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "liquiddock";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprland-plugins/tree/main/liquiddock";
    description = "Hyprland dock plugin with gooey SDF rendering";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
