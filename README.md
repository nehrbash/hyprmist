# hyprmist

Smoke that expands beyond Hyprland window borders. Opening and closing windows
emit clouds; slides and dragging leave trails that follow actual screen movement.
Puffs rise, expand and fade independently after motion stops. Native window and
workspace animation directions, curves and durations are unchanged.

Targets Hyprland **0.56.2 / efb50993780079460b0cbed1363e2166a2de1d9f**.
Build with the matching Hyprland headers and C++ runtime:

```sh
make
make check
```

Load after renderer initialization in `hyprland.lua`:

```lua
hl.on("hyprland.start", function()
    hl.exec_cmd("hyprctl plugin load /absolute/path/to/hyprmist.so")
end)
```

Runtime control:

```sh
hyprctl eval 'hl.plugin.hyprmist.enable(false)'
hyprctl eval 'hl.plugin.hyprmist.enable(true)'
```

The plugin adds textured quads after windows and before top/overlay layers.
It does not replace window rendering or clip particles to their source window.
Smoke may overlap neighbouring windows. Fullscreen windows do not emit smoke;
particles are cleared while the session is locked. At most 600 puffs are alive,
with drawing and damage tracking limited to their bounds. No emission or frame
requests continue after they expire.

HyprWindowShade can supply the accompanying window dissolve/reveal, but is not
required. No shader files or generated image assets are needed: four cloud
textures are generated once, with premultiplied alpha.
