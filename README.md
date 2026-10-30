# hyprmist

Rolling smoke reveals and dissolves Hyprland windows. A shared turbulence field
drives both the cloud and the window mask, extending beyond the window borders.
Rotating eddies open irregular gaps through the cloud as the window emerges.
Workspace effects follow native fade or slide timing; dragging leaves lighter
wisps. HyprWindowShade is not required.

Targets Hyprland **0.56.2 / efb50993780079460b0cbed1363e2166a2de1d9f**.
Experimental: a live compositor crash during plugin activation remains unresolved.
Use the isolated test below while investigating it.

Build with the matching Hyprland headers and C++ runtime:

```sh
make
make check
```

The integration test requires Guile with guile-json, Alacritty, and a running
Wayland session. It starts its own nested compositor with a separate runtime
directory and disables access to DRM outputs. Logs remain in the printed
temporary directory. Optional companion plugins load only into that compositor:

```sh
guile --no-auto-compile tests/nested.scm /absolute/path/to/Hyprland \
    ./hyprmist.so /absolute/path/to/hyprbars.so
```

It checks two outputs, a two-tab group, smoke during workspace fades, twenty
unload/load cycles during animation, configuration reloads, idle cleanup, GPU
mask endpoints, spatial reveal, vortex motion, close snapshots, and disabling
the effect during a close.
Set `HYPRMIST_TEST_CAPTURE` to a PNG path for seven reveal frames plus workspace,
idle, and close captures with `grim`.

Load after renderer initialization in `hyprland.lua`:

```lua
hl.on("hyprland.start", function()
    hl.exec_cmd("hyprctl plugin load /absolute/path/to/hyprmist.so")
end)
```

For windows to emerge in place, use `popin 100%` for `windowsIn` and
`windowsOut`, and `fade` for `workspaces` and `specialWorkspace`. Keep animations
enabled: their fade progress drives the mist. Opening mist lasts 0.6 seconds;
workspace and close effects follow the compositor's animation duration.

Runtime control:

```sh
hyprctl eval 'hl.plugin.hyprmist.enable(false)'
hyprctl eval 'hl.plugin.hyprmist.enable(true)'
```

The plugin uses native window transformers for reveals and masks Hyprland’s close
snapshots from an intact copy. Disabling or unloading removes its transformers
and restores surviving snapshots. It installs no renderer function hooks.

Smoke draws after windows and before top/overlay layers, and can overlap nearby
windows. Fullscreen windows do not emit smoke; effects clear while the session
is locked. Fields render at half resolution, capped at 1024 pixels per axis,
and release when idle. Up to eight close snapshots and 600 drag wisps are kept.
No shader files or generated image assets are required.
