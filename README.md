# hyprmist

Volumetric mist reveals and conceals Hyprland windows. Overlapping billows drift,
expand, and disperse independently. Light passing through the smoke creates
shadowed interiors and small theme-colored highlights. Window visibility changes
beneath dense mist. Opening lasts 0.9 seconds; closing and workspace transitions
follow their native animation progress. Dragging leaves lighter wisps.

Targets Hyprland **0.56.2 / efb50993780079460b0cbed1363e2166a2de1d9f**.
Build with matching Hyprland headers, Lua 5.5, nlohmann-json, and C++23:

```sh
make
make check
```

The volume shader is embedded at build time. No runtime shaders or image assets
are required. `preview/` provides an isolated Quickshell study using the same
shader source; see [its instructions](preview/README.md).

## Colors

Colors follow `$XDG_STATE_HOME/burl/scheme.json` (default
`~/.local/state/burl/scheme.json`). Theme updates are detected during rendering.
Optional overrides live in `$XDG_CONFIG_HOME/hyprmist/colors.json` (default
`~/.config/hyprmist/colors.json`):

```json
{
  "colors": {
    "shadow": "surfaceContainerHighest",
    "light": "onSurface",
    "primary": "primary",
    "secondary": "secondary",
    "tertiary": "tertiary"
  }
}
```

Each value accepts a theme role or `#RRGGBB`. Explicit colors persist across theme
changes. The light color is mostly desaturated for neutral illumination; primary,
secondary, and tertiary provide localized accents. Incomplete JSON writes retain
the last valid palette. Without a theme file, a built-in palette is used.

## Loading and animation

Load after renderer initialization in `hyprland.lua`:

```lua
hl.on("hyprland.start", function()
    hl.exec_cmd("hyprctl plugin load /absolute/path/to/hyprmist.so")
end)
```

For stationary transitions, use `popin 100%` for `windowsIn`/`windowsOut`, `fade`
for `workspaces`/`specialWorkspace`, and a linear curve with speed 9 for closing
and workspace animations. A short `fadeIn` finishes before the opening mist
reveals the window. Native movement remains under Hyprland's control.

```sh
hyprctl eval 'hl.plugin.hyprmist.enable(false)'
hyprctl eval 'hl.plugin.hyprmist.enable(true)'
```

Mapped windows use native transformers. Closing snapshots are copied and rendered
through the mist mask without the native fade's extra opacity reduction. Disabling
or unloading restores surviving snapshots and removes owned transformers. No
renderer function hooks are installed.

Smoke draws after windows and before top/overlay layers and can overlap nearby
windows. Fullscreen windows do not emit; effects clear while the session is locked.
Volume fields are capped at 350,000 pixels and 1024 pixels per axis and release
when idle. Up to eight close snapshots and 600 drag wisps are kept.

## Integration checks

The test needs Guile with guile-json, Alacritty, and a running Wayland session.
It starts a nested compositor with a private runtime directory and no DRM access:

```sh
guile --no-auto-compile tests/nested.scm /absolute/path/to/Hyprland \
    ./hyprmist.so /absolute/path/to/hyprbars.so
```

It checks two outputs, grouped windows, slide/fade workspace animations, twenty
unload/load cycles, reload, idle cleanup, GPU mask endpoints, closing snapshots,
and disable cleanup. Logs remain in the printed temporary directory. Set
`HYPRMIST_TEST_CAPTURE` to a PNG path to capture the test window's output with
`grim`. Palette tests exercise theme changes, overrides, and partial writes.
