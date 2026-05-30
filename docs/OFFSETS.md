# Updating offsets after a game patch

When Combat Master updates, `Project.dll` gets rebuilt and field offsets /
RVAs shift. This doc explains what to check and how to update.

## What's patch-sensitive

| File | Constants | What they reference |
|------|-----------|---------------------|
| `src/il2cpp/offsets.h` | `OFF_PLAYERROOT_*`, `OFF_PLAYERHEALTH_*`, `OFF_PLAYERMOBVIEW_*`, `OFF_BOLTHITBOX_*` | Field byte-offsets inside IL2CPP objects |
| `src/features/cosmetics.cpp` | `kUnlockRVAs[]` (13 entries) | `Is*Available` function addresses relative to Project.dll base |
| `src/features/cosmetics.cpp` | `items[]` in `SaveEquippedToCloud` | 4 `Buy*` RVAs + CharacterStore equipped-item field offsets |
| `src/features/cosmetics.cpp` | `kMaxLevelAvailRVAs[]` (8 entries) | Attachment/camo/detail color availability checks |
| `src/features/cosmetics.cpp` | `kRVA_IsMaxLevel`, `kRVA_GetLevel`, `kRVA_GetMaxLevel` | WeaponStoreData level functions |
| `src/features/cosmetics.cpp` | `charStore` offset (`0x7D8`), `_forceSaveToPlayfab` (`0x1DF4`) | PlayerProfile layout |
| `src/features/combat.cpp` | `kRVA_PlayDamageShake` (`0x33C6470`) | CameraController.PlayDamageShakeAnimation |
| `src/features/exploit.cpp` | `kRVA_GetDeviceUniqueId` (`0x2FA0170`) | SystemInfo.GetDeviceUniqueIdentifier |
| `src/il2cpp/il2cpp.cpp` | `0x2FA9830` (fallback) | `Transform.get_position_Injected` RVA |
| `src/features/combat.cpp` | `0x48`, `0x188`, `0x194`, `0x198`, `0x60`, `0x7C`, `0x80`, `0x34`, `0x38`, `0x3C`, `0x40` | CameraController + ShootUseTypeInfoExt recoil/spread fields |

## What's NOT patch-sensitive

- The 5 obfuscated IL2CPP export names (`ApzldXylDWR`, etc.) — these are baked into the game's linker and only change if the devs re-obfuscate.
- Unity method names resolved via `class_get_method_from_name` — these use string lookups at runtime.
- The heuristic static-field scan range (`0xB0..0x200`) — this is a search, not a hardcoded offset.

## Workflow

1. **Re-dump.** Inject `dumper.dll` into the updated game. Wait 20s for it to finish. Output lands in `dumps/il2cpp_dump.cs`.

2. **Find the classes.** Search the dump for:
   - `class PlayerRoot`
   - `class PlayerHealth`
   - `class PlayerMobView`
   - `class BoltHitbox`
   - `class BoltHitboxBody`
   - `class CharacterStore`
   - `class ArsenalStore`
   - `class PlayerProfile`
   - `class CameraController` (the one in `CombatMaster.Battle.Gameplay.Player.CameraController`)
   - `class ShootUseTypeInfoExt`

3. **Diff field offsets.** Each field line ends with `// 0xNN`. Compare against the constants in `offsets.h` and `cosmetics.cpp`. Update any that shifted.

4. **Diff RVAs.** Each method line ends with `// RVA: 0xNNNNNNN`. Compare the `Is*Available`, `Buy*`, and `get_position_Injected` RVAs against the hardcoded values. Update any that shifted.

5. **Build and test.** `build.bat` — inject into the updated game, verify ESP draws players and cosmetics toggle works.

## Tips

- Field offsets usually shift by a fixed delta when a new field is inserted above them. If `_playerHealth` moved from `0xB8` to `0xC0`, check whether all PlayerRoot offsets shifted by the same `+8`.
- RVAs can shift unpredictably. Always verify each one individually.
- The `CombatMaster_SDK_Full.hpp` in `reference/` is a static snapshot. It's useful for cross-referencing struct layouts but may be outdated relative to the latest dump.
