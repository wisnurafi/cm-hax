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
| `esp.cpp` -> `esp_imgui.dll` | ImGui overlay (D3D11 `Present` hook via MinHook) showcasing ESP, aimbot smoothing, no-recoil, no-spread, and cosmetic byte patches. |
| `CombatMaster_SDK_Full.hpp` | Reference SDK header (~4.7 MB). Used as documentation, not compiled by default. |
| `il2cpp_dump.cs` | Sample dumper output: 21,079 classes / 141,505 methods / 95,267 fields. |
| `imgui/`, `minhook/` | Vendored dependencies for the menu DLL. |

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

- Windows x64
- Visual Studio 2022 or Build Tools 2022 with the `Desktop development with C++` workload (older versions also work; `vswhere` picks the latest).
- MinHook (only needed for the menu DLL). Place it at the repo root:

```text
minhook/
  include/
    MinHook.h
  src/
    hook.c
    buffer.c
    trampoline.c
    hde/
      hde64.c
```

A prebuilt x64 `.lib` (e.g. `minhook\lib\libMinHook.x64.lib`) or a vcpkg install
under `%VCPKG_ROOT%` works too.

## Build

From a normal Command Prompt or PowerShell at the repo root:

```bat
build.bat            :: builds everything
build.bat injector   :: only injector.exe
build.bat dumper     :: only dumper.dll
build.bat menu       :: only esp_imgui.dll  (needs MinHook)
build.bat clean      :: wipes build/
```

Outputs go to `build/`:

```text
build/
  injector.exe
  esp_imgui.dll
  dumper.dll
  CombatMaster_SDK_Full.hpp   (copied if present)
```

If `build.bat menu` fails with a MinHook message, install MinHook as shown
above and rebuild.

## Run

Run `build\injector.exe` as Administrator and pick a target:

- `1` - menu DLL (`esp_imgui.dll`)
- `2` - dumper DLL (`dumper.dll`)
- `q` - quit

The injector polls for `CombatMaster.exe`, gives the process 3 seconds to
initialize, then injects. Command-line shortcuts:

```bat
build\injector.exe --menu
build\injector.exe --dump
```

The dumper sleeps 20 seconds (game finishes IL2CPP init) and then writes its
output to disk.

## Output paths

The DLLs currently write to the developer's desktop (`C:\Users\NullSyntax\Desktop\...`).
On any other machine these `fopen` calls will silently fail. Update these
constants before running on your own box:

- `dumper.cpp` - `LOG_PATH` and `OUT_PATH`
- `esp.cpp` - `ResetLog` / `Log`

A future change should resolve the desktop with `SHGetKnownFolderPath`.

## Updating offsets after a game patch

`esp.cpp` carries a small set of hard-coded field offsets and RVAs that are
tied to a specific build of `Project.dll`:

- Player-related field offsets (`OFF_PLAYERROOT_*`, `OFF_PLAYERHEALTH_*`,
  `OFF_PLAYERMOBVIEW_*`, `OFF_PLAYERMOVEMENT_*`, `OFF_BOLTHITBOX_*`).
- 13 `Is*Available` RVAs in `g_unlockRVAs[]` for the cosmetic unlock patch.
- 4 `Buy*` RVAs and `CharacterStore` field offsets for the cloud-save flow.
- One `Transform.get_position_Injected` RVA used as a fallback when the
  dynamic resolver fails.

After a game patch, regenerate the SDK header (e.g. with Il2CppDumper) and
diff its field offsets and RVAs against the constants in `esp.cpp`. The
dumper has no game-specific offsets, so it survives patches as long as the
mangled export names stay the same.

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
