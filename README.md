# Senator External

A Roblox external overlay tool for Windows x64. Written in C++ with a DirectX 11 overlay rendered through Dear ImGui.

This is the V2 rewrite. The whole thing was rebuilt from scratch with a modular feature system, per-game support profiles, and a cleaner architecture than the original.

> **For educational and research purposes only.** This project reads memory from a running Roblox process to demonstrate how external overlays work. It does not inject anything into the game. Using it in games may violate their Terms of Service. You are responsible for what you do with it.

## What it does

Senator External reads memory from the Roblox player process and draws an overlay window on top of it. The overlay is a separate process, so nothing gets injected into the game itself.

It has a feature system organized into categories: aimbot, ESP/visuals, movement, rage, exploits, lighting, and a few game-specific profiles. Each feature is a self-contained module registered through a central registry, so adding new ones is mostly a matter of dropping in a new file.

### Supported games

The tool detects which game is running and loads the right profile automatically:

| Game | Profile features |
|---|---|
| Phantom Forces | Silent aim, aimbot camera mode |
| Murder Mystery 2 | Game-specific support |
| Lumber Tycoon 2 | Game-specific support |
| Blade Ball | Game-specific support |
| Anime League | Game-specific support |
| Overkill | Game-specific support |

For any other Roblox experience, the universal features still work. The per-game profiles add on top of that.

### Feature categories

- **Aimbot** - FOV-based targeting with camera and mouse modes, smoothing, prediction, team and health checks
- **ESP / Visuals** - Player boxes, names, distance, health bars, mesh-based rendering
- **Movement** - Fly, speed, jump, gravity, tickrate control
- **Rage** - Hitsounds, hit tracers, rapid fire, noclip, spin, hitbox expander, hip height, magic bullet, third person, desync
- **Exploits** - Headless, korblox, anti-AFK, freeze player, FPS caps, display FPS
- **Lighting** - Clock time, exposure, fog, shadows, skybox
- **Triggerbot** - Auto-fire with reaction delay, CPS control, wallcheck
- **Misc** - Stream proof mode, console, config save/load, explorer, avatar manager

## Building

You need Visual Studio 2022 with the C++ workload installed (MSVC v143, Windows 10/11 SDK). The project targets Release x64 only.

### Dependencies

All third-party libraries are bundled in the `ext/` folder. You do not need to download or configure anything separately.

| Library | Used for |
|---|---|
| Dear ImGui | Overlay UI rendering |
| FreeType | Font rendering |
| Clipper2Lib | Polygon clipping for ESP |
| blake3 | Hashing |
| zstd | Compression |
| nlohmann/json | Config and offsets parsing |
| libcurl | HTTP for offset fetching |
| xxhash | Fast hashing |

System libraries (linked via Windows SDK): `d3d11`, `dxgi`, `shlwapi`, `shell32`, `ntdll`.

### Build steps

1. Open `SenatorExternalV2.sln` in Visual Studio 2022.
2. Select the `Release | x64` configuration.
3. Build. The output goes to the `build/` directory.
4. The post-build step copies the `assets/` folder next to the executable automatically.

The build produces `SenatorExternalV2.exe` in `build/`. Run it while Roblox is open.

## How the offsets work

Roblox updates break memory offsets. The tool fetches a fresh `offsets.json` from a remote offset service at startup. The default service is `https://offsets.imtheo.lol`.

If the fetch fails, it falls back to a local `offsets.json` if one exists in the working directory. You can also host your own offset service and point the tool at it by editing `offsets_base_url` in the config.

## Configuration

Configs are stored as JSON files. The tool creates a config directory and ships a `default.json` that you can start from. You can save, load, and delete configs from the in-game menu.

The `autoload.txt` file specifies which config loads automatically on startup.

## Project structure

```
SenatorExternalV2.sln
SenatorExternalV2/
  main.cpp                 Entry point and startup sequence
  ext/                     Bundled third-party libraries
  assets/                  Icons and images used by the UI
  src/
    branding/              Product name, version, visual identity
    cache/                 Game detection, entity caching, body parts
    features/              All features as modular files
      aimbot/
      esp/
      movement/
      rage/
      exploits/
      lighting/
      menu/
      config/
      explorer/
      avatarmanager/
      triggerbot/
      silentaim/
    game/                   Game state, datamodel access, rescan
    gamesupport/            Per-game detection and profiles
    loader/                 Startup loading sequence
    memory/                 Memory read/write abstraction
    menu/                   ImGui menu and keybind system
    Offsets/                Offset definitions and loader
    render/                 DirectX 11 rendering, 3D API, texture cache
    runtime/                Runtime logging
    sdk/                    Roblox SDK abstraction
    settings/               Settings state and per-category config
    ui/                     Support notification UI
    utils/                  Network utilities
    wallcheck/              OBB-based wall checking
```

## License

Apache License 2.0. See [LICENSE](LICENSE).

## Acknowledgements

Offset service by [imtheo](https://offsets.imtheo.lol). Third-party libraries bundled in `ext/`: Dear ImGui, FreeType, Clipper2Lib, blake3, zstd, nlohmann/json, xxhash, libcurl. All credit to their authors.
