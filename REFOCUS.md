# Hyprland-Refocus

Fork of [hyprwm/Hyprland](https://github.com/hyprwm/Hyprland), based on upstream `0.57.0`
(commit `84b5122`).

## Project goals

1. **Change app focus dynamically.** Move focus between running applications on the fly,
   without editing or reloading the configuration.
2. **Change workspace focus dynamically.** Switch or reassign the focused workspace
   (and which monitor it lives on) at runtime.
3. **Open apps on a chosen monitor or workspace.** Launch an application directly onto a
   given monitor or workspace in an ad-hoc way, without writing a window rule into the
   config first.

The common theme: everything that today needs a config edit (window rules, workspace
rules, binds) should also be possible as a one-off, runtime action.

## Existing upstream building blocks

Upstream already has a lot of the plumbing. These are the places to start:

| Area | Location | Notes |
|---|---|---|
| Focus dispatchers (`hl.focus`, `hl.focus.monitor`) | `src/config/lua/bindings/LuaBindingsDispatchers.cpp` (~L1080–1180) | Supports `direction`, `monitor`, `window`, `workspace`, `urgent_or_last`, `last`. |
| Workspace dispatchers (`hl.workspace.*`) | same file (~L1190–1340) | `move` (to monitor), `swap_monitors`, `rename`, `change_id`. |
| Window move (`hl.window.move`) | same file (~L840–910) | Accepts `workspace` / `monitor` selectors. |
| Exec with one-off rules (`hl.dsp.exec_cmd(cmd, rules)`) | same file (`hlExecCmd`, ~L271) | Optional 2nd argument = rules applied to the spawned process only. |
| Exec rule token / env plumbing | `src/config/supplementary/executor/Executor.cpp`, `src/desktop/rule/Rule.cpp` (`markAsExecRule`) | Rule is tied to the PID via an env token; currently non-persistent (see TODO at `Executor.cpp:67`). |
| Window rule engine | `src/desktop/rule/` (`Engine`, `windowRule/WindowRuleApplicator`) | Where exec rules are matched against new windows. |
| Focus state | `src/desktop/state/GlobalWindowController.cpp` | Central focus handling. |
| IPC (hyprctl) | `src/ipc/s1/Commands.cpp` (`dispatchRequest`, `eval`) | `hyprctl dispatch ...` is a shorthand for `hl.dispatch(...)` under the Lua config. |
| Event socket | `src/ipc/s2/` | Useful for reacting to window/workspace events from external tools. |
| Tests | `tests/` (unit, gtest), `hyprtester/` (integration) | New behaviour should get coverage in one of these. |

## Open questions / ideas to evaluate

- How far does upstream already get us? E.g. can
  `hyprctl dispatch 'hl.dsp.exec_cmd("kitty", { workspace = "3 silent" })'` or a monitor
  rule already cover goal 3? Document gaps before writing code.
- Persistent exec rules: the `persistent` flag in `markAsExecRule` is unused
  (`Executor.cpp:67` TODO). Needed if an app should keep landing on the chosen
  workspace/monitor when it spawns further windows.
- Matching by app class instead of PID, for apps that are launched via a launcher/daemon
  (PID of the spawned process ≠ PID of the window).
- A simpler user-facing interface: a small CLI (`hyprctl` subcommand or separate tool) or
  a picker UI, e.g. `refocus open firefox --monitor DP-1 --workspace 4`.
- "Focus app" by class/title with fallback to launching it if not running
  (run-or-raise).
- Runtime-only rules that can be listed and removed again without touching the config.

## Investigation: touch input vs. seat grabs

**Symptom:** a grab set up via the mouse (an xdg popup/menu, or a
`hyprland_focus_grab_v1` grab from a bar or launcher) also catches touch input.
The first touch outside the grab is lost or delivered to the wrong app.

### Trace (touch down while a seat grab is active)

1. `CInputManager::onTouchDown` (`src/managers/input/Touch.cpp`) calls
   `refocus(TOUCH_COORDS)` and then routes the touch to the surface stored in
   `m_foundWindowToFocus` / `m_foundSurfaceToFocus` / `m_foundLSToFocus`.
2. `refocus()` calls `mouseMoveUnified(0, refocus = true, mouse = false, TOUCH_COORDS)`
   (`src/managers/input/InputManager.cpp`). It first **resets** the three
   `m_found*ToFocus` members (~L279).
3. It finds the surface under the finger. If a grab is active and does not accept that
   surface (~L564), `refocus == true` makes it call `g_pSeatManager->setGrab(nullptr)`
   and **return early**, before `m_found*ToFocus` is set (~L626).
4. `CSeatManager::setGrab(nullptr)` (`src/managers/SeatManager.cpp`, ~L756) ends the
   grab and refocuses. It knows nothing about touch:
   - `input:follow_mouse = 1` (default): calls `g_pInputManager->refocus()` **without a
     position**, i.e. at the **mouse cursor**. This nested call fills `m_found*ToFocus`
     with whatever is under the cursor.
   - `follow_mouse = 0/2/3`: focuses the popup parent or `refocusLastWindow()`. That
     never sets `m_found*ToFocus`, so they stay empty.
5. Back in `onTouchDown`:
   - `follow_mouse = 1`: the touch down is sent to **the surface under the mouse
     cursor**, possibly on another monitor. Its surface-local coordinates come from the
     finger position, so they are wrong too. Every later motion and up event of that
     finger follows it there.
   - otherwise: nothing is found, and the touch is dropped (`return; // oops, nothing found.`).

Touches *inside* the grab work. After the first outside touch the grab is gone, so later
touches work again. Mouse and touch share one grab: `CSeatGrab` only has `m_keyboard` /
`m_pointer` flags, and the only grab check sits in the pointer path (`mouseMoveUnified`),
which touch reuses.

For comparison, the mouse: a press outside the grab is sent to the grabbed client (pointer
focus is held on the grab), and then `simulateMouseMovement()` with `m_hardInput` ends the
grab (`processMouseDownNormal`, ~L942). The click outside is effectively **consumed**; the
app underneath never sees it.

### Decision: touch is fully separate from the pointer

Touch always goes to the surface displayed under the finger. Mouse grabs (popups, focus
grabs) stay with the mouse and are neither ended nor obeyed by touch.

Implemented on `claude/hyprland-fork-setup-49phcb`:

- The pure hit-test part of `mouseMoveUnified` (layer popups, overlay/top layers,
  fullscreen handling, windows, bottom/background layers) moved unchanged into
  `CInputManager::pickDesktopSurface()`. The mouse path calls it at the same point, so its
  behaviour does not change.
- `CInputManager::touchSurfaceAt()` (`Touch.cpp`) picks the touch target: IME popup →
  focus-priority window → (session lock: above-lock layers) → `stayfocused` window →
  `pickDesktopSurface()`. It does **not** look at seat grabs, held mouse buttons,
  pointer constraints or an ongoing mouse window drag, and never changes pointer focus or
  the cursor.
- `CInputManager::focusTouchTarget()` gives keyboard focus to the touched window/layer as
  before, **except** while a seat grab that does not include that surface is active. Then
  the grab keeps the keyboard and the touch is only delivered as a touch.
- `onTouchDown` no longer calls `refocus()`. The `m_found*ToFocus` members (only used by
  touch) are gone.

Side effects of the separation (all intended):

- A touch outside an open popup no longer closes it.
- Touch also works while a mouse button is held, while the pointer is locked/confined by
  a game (pointer constraints), and during a mouse window drag. Before, it was dropped in
  those cases.
- Touch no longer emits the `mouse.move` event bus signal and no longer moves pointer focus.
- Unchanged: `stayfocused` windows still take touch (config behaviour); touch
  drag-and-drop still uses `refocus()` in `onTouchMove`; workspace swipe; session lock.

Still open:

- Not compiled yet (the cloud environment lacks the dependencies). Build and test locally.
- Regression test in `hyprtester/`: needs a client with an xdg popup grab that logs
  `wl_touch.down` per surface, and a test-plugin call that touches at an arbitrary (x, y)
  (`hyprtester/plugin/src/WorkspaceGestures.cpp` only does x with y = 0.5).
- `hl.dsp.kill` click mode via touch still kills the window under the **mouse cursor**
  (`processMouseDownKill` uses `getMouseCoordsInternal()`).

## Development setup

### Branches and remotes

- `main` – mirror of upstream at fork time.
- `claude/hyprland-fork-setup-49phcb` – current working branch.
- `upstream` remote → `https://github.com/hyprwm/Hyprland` (for pulling upstream changes
  with `git fetch upstream`).

### Building

Hyprland needs the hypr* ecosystem libraries (`hyprutils`, `hyprlang`, `aquamarine`,
`hyprcursor`, `hyprgraphics`, ...) plus Wayland, libdrm, EGL/GBM, xkbcommon and Lua.
See the upstream wiki: https://wiki.hypr.land/Getting-Started/Installation/

```sh
git submodule update --init --recursive
make debug        # or: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build
```

Testing a debug build in a nested session: run `./build/Hyprland` from inside a running
Hyprland session (it opens as a window).

### CI (GitHub Actions)

`.github/workflows/ci.yaml` runs on every push (and on PRs from forks), in an Arch container:

- **Code style:** `make format-check` with Arch's current clang-format. Fix locally with
  `make format-fix`; an older clang-format rejects keys in `.clang-format`.
- **Build & unit tests:** release build with `-Werror` and tests enabled, then
  `build/hyprland_gtests`. The binaries are uploaded as a workflow artifact (kept 14 days),
  so a change can be tried without building it locally.
- **Integration tests (hyprtester):** Hyprland needs a DRM device for buffer allocation
  even in headless mode, and GitHub runners have no GPU. The job loads the `vkms` virtual
  display driver on the runner, then builds (Debug) and runs hyprtester as a normal user in
  a privileged Arch container with Mesa software rendering and `seatd`
  (`.github/scripts/hyprtester.sh`). Upstream runs the same tests in a Nix VM with a
  virtio GPU instead. Logs (hyprtester and Hyprland) are uploaded as an artifact.

Dependency setup is shared by both jobs: `.github/scripts/setup-deps.sh`.

The Hypr libraries (hyprutils, aquamarine, hyprgraphics, hyprcursor, hyprlang, hyprwire,
hyprwayland-scanner) are built from the revisions pinned in `flake.lock`
(`.github/actions/setup_base`). Upstream builds them from their latest `main`, which
would drift away from this fork's 0.57.0 base. After merging upstream, update
`flake.lock` (e.g. take upstream's) so CI follows.

Not run in CI: Nix builds. Removed upstream-only workflows: issue/PR/discussion moderation, vouching,
code owners, labeler, commit-title check, AI translation check, man-page auto-commit,
release tarballs, Nix input updates.

### Code style

Follow `AGENTS.md` (clang-format, naming `CClass` / `SStruct` / `m_member`, hyprutils
smart pointers and casts, tests for new behaviour).

## Upstream contribution note

If anything from this fork is ever proposed upstream, Hypr's AI usage policy applies:
https://github.com/hyprwm/.github/blob/main/policies/AI_USAGE.md and the issue/discussion
guidelines: https://wiki.hypr.land/contributing-and-debugging/issue-guidelines/.
PRs, issues and discussions towards hyprwm must be written and submitted by a human.
