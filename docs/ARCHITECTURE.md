# Architecture

High-level overview of how Cm-Hax works at runtime.

## Artifacts

| Output | Source | Role |
|--------|--------|------|
| `injector.exe` | `injector/main.cpp` | Polls for `CombatMaster.exe`, elevates privileges, injects a chosen DLL via `CreateRemoteThread` + `LoadLibraryA`. |
| `dumper.dll` | `dumper/main.cpp` | Internal IL2CPP metadata dumper. Attaches to the runtime domain, walks all assemblies/classes/fields/methods, writes a C#-style dump to disk. |
| `cm_hax.dll` | `src/` | The menu DLL. Hooks D3D11 `Present`, renders an ImGui overlay, and drives ESP, aimbot, triggerbot, combat patches, and cosmetic unlocks. |

## Boot sequence (`cm_hax.dll`)

```
DllMain (DLL_PROCESS_ATTACH)
  └─ CreateThread → Hooks::MainThread
       ├─ Wait for Project.dll (renamed GameAssembly.dll)
       ├─ IL2CPP::ResolveAll
       │    ├─ GetProcAddress for 5 obfuscated exports
       │    ├─ ResolveCoreModule (Camera, Transform, CharacterController)
       │    └─ ResolveBattleModule (PlayerRoot, PlayerHealth, BattleExtensions)
       ├─ Hooks::PollStaticFields (first attempt)
       │    ├─ ScanPlayerRootStatics (heuristic 0xB0..0x200)
       │    └─ ScanGameClientStatics (same pattern)
       ├─ D3D11 dummy swapchain → vtable steal → MH_CreateHook Present/ResizeBuffers
       ├─ CreateThread → ESP::DataThread
       └─ Config::Load
```

## Runtime threads

| Thread | Role |
|--------|------|
| **MainThread** | One-shot bootstrap. Exits after hooking. |
| **ESP DataThread** | 60 Hz loop. Collects player data (positions, bones, health, visibility). Retries `PollStaticFields` every 500ms until resolved. Applies combat patches (no-recoil/spread). Writes snapshot to `Runtime::g_espData` under a critical section. |
| **Present hook** (game's render thread) | Reads the ESP snapshot. Runs ImGui frame: menu, ESP visuals, aimbot target picking, triggerbot detection, overlay. |

## Data flow

```
ESP DataThread                          Present Hook (render thread)
─────────────                           ────────────────────────────
PlayerRoot.AllPlayers list
  → filter team/dead/downed
  → resolve bones, hitboxes, HP
  → Combat::ApplyWeaponPatches
  → snapshot into g_espData ──CS──→  copy g_espData locally
                                       → project W2S per player
                                       → draw ESP visuals (if espEnabled)
                                       → pick aim target → Aim::Apply
                                       → detect crosshair-on-bbox → Trigger::OnFrame
                                       → Menu::Draw (ImGui)
                                       → Overlay::Draw (FPS pill)
```

## Directory map

```
Cm-Hax/
├── injector/           injector.exe source
├── dumper/             dumper.dll source
├── src/                cm_hax.dll source
│   ├── dllmain.cpp
│   ├── core/           globals, config, hooks, logging
│   ├── il2cpp/         IL2CPP runtime: exports, player wrappers, offsets, SDK structs
│   ├── features/
│   │   ├── aimbot/     mouse smoothing, hitbox resolver
│   │   ├── triggerbot/ non-blocking LMB click state machine
│   │   ├── esp.*       data collection thread
│   │   ├── combat.*    no-recoil / no-spread patcher
│   │   └── cosmetics.* Is*Available byte patches + cloud save
│   ├── render/         ImGui menu, ESP drawing, overlay
│   └── utils/          math, memory, strings, input helpers
├── third_party/        imgui + minhook (fetched by build.bat setup)
├── reference/          SDK header (documentation, not compiled)
├── dumps/              runtime output from dumper.dll (gitignored)
├── docs/               this file + OFFSETS.md
└── build.bat           one-command build system
```

## Key design decisions

- **Single-struct state (`CmState`)**: all toggles, colors, and runtime status live in one global. Simple, cache-friendly, easy to serialize. Future refactor will split into per-feature sub-structs.
- **Critical section for ESP data**: the data thread writes a full snapshot atomically; the render thread copies it once per frame. No per-field locking, no torn reads.
- **Heuristic static-field scan**: IL2CPP doesn't expose a stable API for reading a class's static block pointer. We walk the class object at known offsets and validate candidates by checking list structure. Self-heals from the data thread if the initial scan runs too early.
- **`__try/__except` everywhere**: IL2CPP pointers can go stale between frames (player disconnects, GC moves objects). SEH guards prevent crashes; failed reads just skip the player.
- **Non-blocking triggerbot**: no `Sleep()` in the render thread. Click timing is driven by `GetTickCount()` comparisons across frames.
