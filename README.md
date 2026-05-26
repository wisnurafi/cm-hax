# Cm-Hax

Internal IL2CPP tooling for **Combat Master** (Steam, free-to-play FPS) built on
Unity `6000.0.60`. The project ships an injector, a runtime metadata dumper and
an ImGui menu DLL that demonstrates how the obfuscated IL2CPP exports can be
resolved at runtime.

This repo exists as a learning resource for IL2CPP reverse engineering. See the
[Disclaimer](#disclaimer) before doing anything with it.

## What's in here

| Path | Description |
|------|-------------|
| `injector.cpp` -> `injector.exe` | Waits for `CombatMaster.exe`, raises `SeDebugPrivilege`, classic `CreateRemoteThread` + `LoadLibraryA` injection. |
| `dumper.cpp` -> `dumper.dll` | Internal IL2CPP dumper. Walks domain -> assemblies -> images -> classes -> fields/methods and writes a C#-style file. |
| `src/` -> `cm_hax.dll` | Modular menu DLL (D3D11 `Present` hook + ImGui overlay). ESP, aimbot, no-recoil, no-spread, cosmetic byte patches. |
| `CombatMaster_SDK_Full.hpp` | Reference SDK header (~4.7 MB). Used as documentation, not compiled by default. |
| `il2cpp_dump.cs` | Sample dumper output: 21,079 classes / 141,505 methods / 95,267 fields. |
| `imgui/`, `minhook/` | Vendored dependencies for the menu DLL. |
| `Prompt That I Used.txt` | The prompt that drove the dumper design (struct layouts and obfuscated export map). |
| `third_party/imgui`, `third_party/minhook` | Vendored dependencies for the menu DLL. |

## Source layout (`src/`)

```
src/
  dllmain.cpp                 entry point + main-thread bootstrap mutex
  core/
    globals.{cpp,h}           CmState (toggles, colors, runtime status)
    config.{cpp,h}            JSON-style save/load + AppData migration
    hooks.{cpp,h}             D3D11 Present/ResizeBuffers, WndProc, init/unload
    logging.{cpp,h}           lightweight desktop-file logger
  game/
    sdk.h                     IL2CPP runtime structs (Object/List/Array/String)
    offsets.h                 PlayerRoot/PlayerHealth/MobView field offsets
    il2cpp.{cpp,h}            export resolution + Unity/Battle method lookup
    player.{cpp,h}            PlayerData + safe IL2CPP wrappers + bone capture
  aimbot/
    hitbox.{cpp,h}            HitboxSpec table + honest resolver (no remap)
    aimbot.{cpp,h}            mouse smoothing/stickiness + sticky-lock state
  features/
    esp.{cpp,h}               60Hz collector thread (PlayerRoot list -> shared)
    combat.{cpp,h}            no-recoil / no-spread per-frame patcher
    cosmetics.{cpp,h}         Is*Available byte patches + cloud save
  render/
    menu_style.{cpp,h}        palette + ImGui style + Segoe UI font loading
    menu_widgets.{cpp,h}      Toggle / Pill / SidebarButton / SectionHeader
    menu.{cpp,h}              window chrome + per-tab renderers
    esp_render.{cpp,h}        W2S projection + DrawESP + aim target picking
    overlay.{cpp,h}           subtle FPS pill (foreground draw list)
  utils/
    math.{cpp,h}              Vector3, Matrix4x4, ClampFloat, Distance3D
    memory.{cpp,h}            IsPlausiblePtr, BytePatch
    strings.{cpp,h}           Il2CppString -> UTF-8 helper
```
>>>>>>> 38cbc64 (Refactor into modular src/ tree, add one-command build, scrub personal paths)

## How the IL2CPP API gets resolved

Combat Master ships `GameAssembly.dll` renamed to `Project.dll`, with the
IL2CPP exports renamed to opaque 11-character symbols. The tools resolve them
at runtime via `GetProcAddress(hProject, "<mangled>")`. A few examples:

| Real name | Mangled export |
|-----------|----------------|
| `il2cpp_domain_get` | `ApzldXylDWR` |
| `il2cpp_domain_get_assemblies` | `gobBtMyApTj` |
| `il2cpp_thread_attach` | `ZxbKYsbhTwf` |
| `il2cpp_image_get_class` | `sEfnFqjmWJN` |
| `il2cpp_image_get_class_count` | `eTLADjuMyXv` |
| `il2cpp_class_get_methods` | `wwlPDayBOUV` |
| `il2cpp_class_get_fields` | `dvnHzpFiKhe` |
| `il2cpp_class_from_name` | `etIgYqMGBTj` |
| `il2cpp_class_get_method_from_name` | `BfmNBPOFdWH` |
| `il2cpp_type_get_name` | `ySfCbKcrsLP` |

The full mapping lives in `Prompt That I Used.txt`.

## Requirements

- Windows x64.
- [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) **or**
  the standalone [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/visual-cpp-build-tools/),
  with the **Desktop development with C++** workload installed.
- [Git for Windows](https://git-scm.com/download/win) on `PATH` (only needed
  the first time, to fetch the third-party dependencies).

The build script auto-detects the latest installed Visual Studio via `vswhere`,
and falls back to common install paths.

## Quick start (one-command build)

```bat
build.bat setup        :: clones ImGui + MinHook into third_party/  (run once)
build.bat              :: Release build of injector + cm_hax.dll + dumper.dll
build\Release\injector.exe
```

That's the entire flow. No need to open Visual Studio.

## Build script reference

```bat
build.bat                    :: all targets, Release (default)
build.bat all                :: same as above
build.bat menu               :: only cm_hax.dll
build.bat dumper             :: only dumper.dll
build.bat injector           :: only injector.exe
build.bat <target> debug     :: Debug build of <target>
build.bat <target> release   :: Release build of <target> (explicit)

build.bat setup              :: clone third_party/ deps if missing (idempotent)
build.bat package            :: Release build + portable zip in release/
build.bat clean              :: wipe build/

build.bat help               :: print this list
```

Outputs are split per configuration:

```
build/
  Release/
    injector.exe
    cm_hax.dll
    dumper.dll
    CombatMaster_SDK_Full.hpp   (copied if present)
    obj/                        (intermediate .obj files)
  Debug/
    cm_hax.dll
    cm_hax.pdb
    ...
release/
  Cm-Hax_<YYYYMMDD>/            (staged folder, ready to copy)
    injector.exe
    cm_hax.dll
    dumper.dll
    README.md
    USAGE.txt
  Cm-Hax_<YYYYMMDD>.zip
```

`build.bat package` is the easiest way to hand the cheat to a tester: the
zip is fully self-contained.

## Manual dependency setup (if you don't want `setup`)

If you'd rather pin specific versions, drop the dependencies in by hand:

```text
third_party/imgui/         (https://github.com/ocornut/imgui, v1.91.6 tested)
  imgui.h, imgui.cpp, ...
  backends/
    imgui_impl_win32.cpp + .h
    imgui_impl_dx11.cpp  + .h

third_party/minhook/       (https://github.com/TsudaKageyu/minhook, v1.3.3 tested)
  include/MinHook.h
  src/hook.c, buffer.c, trampoline.c
  src/hde/hde64.c
```

A prebuilt x64 `.lib` (e.g. `third_party/minhook/lib/libMinHook.x64.lib`) or
a vcpkg install under `%VCPKG_ROOT%` works too. The build script tries each
location in order.

## Run

Run `build\Release\injector.exe` as Administrator and pick a target:

- `1` - menu DLL (`cm_hax.dll`)
- `2` - dumper DLL (`dumper.dll`)
- `q` - quit

The injector polls for `CombatMaster.exe`, gives the process 3 seconds to
initialize, then injects. Command-line shortcuts:

```bat
build\Release\injector.exe --menu
build\Release\injector.exe --dump
```

In-game keys:

- `INSERT` - toggle the menu
- `END` - unload the DLL
- `RMB` (default) - aim while held; rebindable in the Aim tab

The dumper sleeps 20 seconds (game finishes IL2CPP init) and then writes its
output to disk.

## Config & output paths

`cm_hax.dll` persists user settings to `%APPDATA%\CmHax\cm_hax.cfg` (folder
auto-created). On first run, an old config sitting next to the DLL will be
migrated automatically.

Both DLLs write log/dump files to the current user's desktop:

- `cm_hax.dll` -> `%USERPROFILE%\Desktop\cm_hax_log.txt`
- `dumper.dll` -> `%USERPROFILE%\Desktop\dumper_log.txt` and `%USERPROFILE%\Desktop\il2cpp_dump.cs`

The `il2cpp_dump.cs` file is generated at runtime by `dumper.dll`. It isn't
committed to the repo because it's regenerable, large (~28 MB), and goes
stale every game patch - re-inject the dumper to refresh it.

## Updating offsets after a game patch

`src/game/offsets.h` and a few RVAs inside `src/features/cosmetics.cpp` are
tied to a specific build of `Project.dll`:

- Player-related field offsets (`OFF_PLAYERROOT_*`, `OFF_PLAYERHEALTH_*`,
  `OFF_PLAYERMOBVIEW_*`, `OFF_BOLTHITBOX_*`).
- 13 `Is*Available` RVAs in `kUnlockRVAs[]` (cosmetic unlock patch).
- 4 `Buy*` RVAs and `CharacterStore` field offsets in `Cosmetics::SaveEquippedToCloud`.
- One `Transform.get_position_Injected` RVA used as a fallback when the
  dynamic resolver fails (`src/game/il2cpp.cpp`).

After a game patch, regenerate the SDK header (e.g. with Il2CppDumper) and
diff its field offsets and RVAs against the constants in `src/game/offsets.h`
and `src/features/cosmetics.cpp`. The dumper has no game-specific offsets,
so it survives patches as long as the mangled export names stay the same.

## Disclaimer

This project is published for **IL2CPP reverse-engineering and educational
purposes**. It is not endorsed by, affiliated with, or supported by Alfa Bravo
Inc. or the Combat Master developers.

- Loading these DLLs into the live game **violates Combat Master's Terms of
  Service** and will, sooner or later, result in account and hardware bans.
- Distributing modified game binaries or running this against online services
  may violate computer-misuse / anti-circumvention laws in your jurisdiction.
- The code is provided **as-is, with no warranty**. You are solely responsible
  for what you do with it.

If you use any of this, do it on isolated test accounts, against offline /
private builds, or for static analysis only.
