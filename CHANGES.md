# Changes against upstream Hyprland

Everything Hyprland-Refocus changes on top of upstream Hyprland. Keep this list up to date with
every change that lands on `main`.

Base: upstream `main` at `60a66a1` (24 Nov 2026, reports version `0.57.0`), merged on top of the
original base `84b5122` (15 Nov 2026). Upstream `main` is ahead of the 0.57.x releases: all fixes of
0.57.1 and 0.57.2 were already in `84b5122` (checked, e.g. `seat: fix grab check (#16900)`). The
upstream differences to a release are not listed here; see them with
`git log --oneline v0.57.2..60a66a1` in a clone that has upstream's tags.

Merging upstream: `git fetch upstream && git merge upstream/main`. Usual conflicts are in files the
fork replaced or removed (`.github/actions/setup_base/action.yml`: keep the fork's version and apply
upstream's package changes to `.github/scripts/setup-deps.sh`; removed workflows stay removed) and in
the hyprtester plugin's source list (`hyprtester/plugin/Makefile`, `src/main.cpp`: keep both).
Take upstream's `flake.lock` so CI builds the matching Hypr libraries.

## Compositor behaviour

### Touch input is separate from the mouse

`src/managers/input/Touch.cpp`, `src/managers/input/InputManager.cpp`, `InputManager.hpp`

- A touch always goes to the surface under the finger. Seat grabs made with the mouse (xdg
  popups, focus grabs from bars and launchers) neither block touches nor get ended by them.
- A touch never moves pointer focus or changes the cursor.
- Keyboard focus follows the tap, except while a grab that doesn't include the tapped surface is
  active.
- Fixed: a touch outside an open popup was delivered to the surface under the mouse cursor
  (`follow_mouse = 1`) or silently dropped (other `follow_mouse` modes).
- Behaviour changes:
  - a tap outside an open popup no longer closes it;
  - touch works while a mouse button is held, while the pointer is locked or confined, and
    during a mouse window drag;
  - touch no longer emits the `mouse.move` event bus signal.
- Unchanged: `stayfocused` windows still take touch; touch drag-and-drop; touch workspace swipe;
  session lock. Touch kill mode still uses the mouse position (open item).
- The surface hit-test of `mouseMoveUnified` moved unchanged into `pickDesktopSurface()`; mouse
  behaviour is identical.

### `exec_cmd`: optional `match` table

`src/config/lua/bindings/LuaBindingsInternal.cpp`, `src/desktop/rule/windowRule/WindowRule.cpp`,
`WindowRule.hpp`

```lua
hl.dsp.exec_cmd("firefox", { workspace = "3", monitor = "DP-1", match = { class = "firefox" } })
```

- The one-off rule also takes the first new window matching `match` (same keys as
  `hl.window_rule`). For apps that hand a new window to an instance that is already running.
- Still one-shot, still expires after one minute. Without `match` nothing changes.
- Errors: unknown keys, wrong value types, an empty `match`, a `match` without any effect.

### `hl.refocus`

`src/config/lua/bindings/LuaBindingsToplevel.cpp`

- `{ api = 1 }` on the fork, `nil` on upstream, so a config shared with upstream can guard
  fork-only features (`if hl.refocus then ... end`). `hl.version()` is unchanged.

## Build and install

- CMake option `REFOCUS_SESSION` (off by default): installs a "Hyprland Refocus" login entry to
  `/usr/share/wayland-sessions` (`REFOCUS_SESSION_DIR`). It keeps `DesktopNames=Hyprland` and
  starts this build's binary with `start-hyprland --path`. Template:
  `example/hyprland-refocus.desktop.in`.
- `start-hyprland`: the NixGL check asks the binary it will actually start
  (`start/src/helpers/Nix.cpp`).
- Lua type stubs: the `exec_cmd` signature includes `match` (`meta/generateLuaStubs.py`).

## Tests (hyprtester)

- New:
  - `main:touchIgnoresMouseGrab` (touch with and without a mouse grab; plugin fixture
    `hyprtester/plugin/src/TouchGrab.cpp`);
  - `main:execRulesMatchFallback`, `main:execRulesMatchFallbackMonitor`;
  - `main:refocusMarker`.
- Fixed upstream flakiness:
  - the `child-window` test client flushes its Wayland requests after each command;
  - `clients:surfaceScaleTransform` waits for the client's initial scale/transform report.

## CI and repository

- New `.github/workflows/ci.yaml`: code style; build with `-Werror` and unit tests, with the Hypr
  libraries pinned to `flake.lock` (`.github/scripts/setup-deps.sh`); hyprtester on a `vkms`
  virtual display in an Arch container (`.github/scripts/hyprtester.sh`); a check of the
  generated login entry. One run per push.
- Removed upstream-only workflows: vouching, code owners, discussion moderation, labeler,
  commit-title check, AI translation check, man-page updates, release tarballs, Nix.
- `REFOCUS.md`: project notes and docs. `AGENTS.md`: fork rules (attribution, pushing); the
  bogus `ZAPI_VALIDATE` paragraph is removed. `CHANGES.md`: this file.
