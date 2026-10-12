# Contributing to Senator External

## Building

Visual Studio 2022 with the C++ workload (MSVC v143, Windows 10/11
SDK). Open `SenatorExternalV2.sln`, select `Release | x64`, build.
All third-party libraries are bundled in `ext/` — nothing to install
separately. Full steps are in the README.

## Adding a feature

Features are self-contained modules under
`SenatorExternalV2/src/features/`, registered through a central
registry. To add one:

1. Create the module file in the right category folder
   (`aimbot/`, `esp/`, `movement/`, ...).
2. Register it so the menu and config system pick it up.
3. Wire its settings into the per-category config — features without
   save/load support don't merge.

Look at an existing small feature first (something in `misc/` is
usually the clearest example) and copy the pattern.

## Per-game profiles

Game-specific behavior lives in `src/gamesupport/`. The tool detects
the running game and loads the matching profile. If you add support
for a new title, keep the universal features untouched — profiles add
on top, they don't fork the core.

## Rules

- **External only, always.** Nothing in this project injects into a
  game process. Any PR that injects, hooks, or patches game code gets
  closed.
- One change per PR. Small PRs get reviewed fast.
- Commit messages read like a human wrote them: `fix: ...`,
  `feat: ...`.
- Don't commit build output. `build/` is gitignored — keep it that way.
- Offsets come from the offset service or a local `offsets.json`.
  Don't hardcode offsets into the source.

## Reporting issues

Include: Windows version, which game was running, the tool version,
and what happened vs. what you expected. Game bans are not bugs —
don't open issues about them.
