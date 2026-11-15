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

### Code style

Follow `AGENTS.md` (clang-format, naming `CClass` / `SStruct` / `m_member`, hyprutils
smart pointers and casts, tests for new behaviour).

## Upstream contribution note

If anything from this fork is ever proposed upstream, Hypr's AI usage policy applies:
https://github.com/hyprwm/.github/blob/main/policies/AI_USAGE.md and the issue/discussion
guidelines: https://wiki.hypr.land/contributing-and-debugging/issue-guidelines/.
PRs, issues and discussions towards hyprwm must be written and submitted by a human.
