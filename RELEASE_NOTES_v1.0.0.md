# Senator External v1.0.0

First public release.

Senator External is a Roblox external overlay for Windows x64. It reads memory from the Roblox player process and renders a DirectX 11 overlay on top, without injecting anything into the game.

## What's in this release

**Core**
- DirectX 11 overlay with Dear ImGui
- Memory read/write abstraction layer
- Remote offset fetching with local fallback
- Config save/load system with autoload support
- Game auto-detection based on place and game ID

**Per-game support for 6 titles**
- Phantom Forces (silent aim, camera aimbot)
- Murder Mystery 2
- Lumber Tycoon 2
- Blade Ball
- Anime League
- Overkill

For any other Roblox experience, the universal features still work.

**Features**
- Aimbot with FOV targeting, camera/mouse modes, smoothing, prediction, team and health checks
- ESP with player boxes, names, distance, health bars, mesh-based rendering
- Movement: fly, speed, jump, gravity, tickrate
- Rage: hitsounds, hit tracers, rapid fire, noclip, spin, hitbox expander, hip height, magic bullet, third person, desync
- Exploits: headless, korblox, anti-AFK, freeze player, FPS caps, display FPS
- Lighting: clock time, exposure, fog, shadows, skybox
- Triggerbot with reaction delay, CPS control, wallcheck
- Stream proof mode, in-app console, explorer, avatar manager

## Requirements

- Windows 10 or 11, x64
- Roblox player installed
- Visual Studio 2022 with C++ workload (MSVC v143, Windows 10/11 SDK) if building from source

## Building from source

1. Clone the repo
2. Open `SenatorExternalV2.sln` in Visual Studio 2022
3. Select `Release | x64`
4. Build

All dependencies are bundled in `ext/`. No external setup needed.

## Notes

This is an educational project. It demonstrates how external overlays and memory reading work. Using it in online games may violate their Terms of Service. Use your own judgment.
