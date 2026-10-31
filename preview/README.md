# Volumetric mist study

Run `quickshell -p preview/shell.qml` from the Hyprmist checkout.
The preview starts paused. Reveal creates a new seeded arrangement; Conceal
reuses it. New arrangement changes the layout without playing. Wide, Tall, and
Compact exercise fitting. Slow motion stretches 0.9 seconds to 2.7 seconds.

`../shaders/volume.glsl` ray-marches a 3D density field made from twelve overlapping
billows with fractal surfaces. Each volume drifts, expands, and loses density on a separate seeded schedule.
The backing layer clears locally after the window transition, leaving drifting
pockets rather than a persistent central mass. Light comes
from the upper left; samples toward the light account for intervening smoke.
This is procedural volume rendering, not a fluid solver.

The smoke renders to a reduced-resolution texture. `composite.frag`
composites it over the full-resolution window, keeping text sharp. Both shaders
must use the same arrival field: the window changes visibility beneath opaque
smoke. The window stays stationary.

Colors follow `$XDG_STATE_HOME/burl/scheme.json`, falling back to
`~/.local/state/burl/scheme.json`, and update when the theme changes.
`options.json` maps shadow, light, primary, secondary, and tertiary to theme roles.
Each can instead contain a `#RRGGBB` override that persists across theme changes.
The body and primary illumination are nearly neutral. Theme accents appear
in localized, illuminated thin regions; shadows retain the shadow color.

Build the preview shaders with `make -C preview` (requires Qt Shader Tools).
Both wrappers compile the same `shaders/volume.glsl` used by the plugin. Restart
the preview after shader rebuilds to clear Qt's shader cache.

The `mistPreview` IPC target exposes `benchmark` and `timings` for collecting
frame-swap timestamps during one animation, plus `capture`, `arrange`, `mode`,
`windowShape`, and `progress` for repeatable visual checks. Frame timings are
local preview measurements, not compositor integration benchmarks.

This does not load the Hyprland plugin. No image assets are used. Preview files
are not part of the installed package.
