# Arcane Activation

Arcane Activation integrates Requiem - Magic Redone's five Alteration
lock-opening spells with ordinary container and door activation.

- Activate a locked container or door to use the cheapest learned spell tier
  that can open it.
- Key-required locks, activation-blocked references, actors, and unlocked
  objects are left alone.
- Activate passes through unchanged when no sufficient spell is known. If the
  player knows one but lacks the required Magicka, the press is consumed and a
  notification explains why.
- The target captured on the initial press remains authoritative. Looking away
  during the animation cannot redirect the unlock. A door unlocks without
  opening or transporting the player; activate it again to open or enter.

## Casting architecture

Arcane Activation temporarily adds its own
marker spell, which makes Open Animation Replacer select the packaged aimed
left-hand casting animation in both first and third person. It adds a separate
one-second ability using Spell Hotbar 2's proven left-hand `AlterationGreen`
art and sends `ShoutStart`. Unlock timing and animation timing then run as two
deadlines in the same captured-target transaction. The configured delay controls
the Requiem unlock call. `MT_BreathExhaleShort` waits at least 450 milliseconds
so an instant unlock cannot release the animation graph a few milliseconds
after `ShoutStart`.

When the player's weapons are drawn, Skyrim can suppress hand-attached art.
Arcane Activation detects that weapon state and also applies the same green
art to the captured target for 1.1 seconds. Sheathed casts use only the
left-hand effect. The plugin logs the chosen path as `CAST_VISUAL_ROUTE`.

The marker, visual ability, magic effect, and art object are dedicated Arcane
Activation records. The DLL does not equip a spell, change either hand, draw
or sheath weapons, drive a `MagicCaster`, access Spell Hotbar state, or alter
shared animation globals.
Both temporary spells are removed through the same cleanup path after success,
interruption, a paused menu, timeout, or validation failure. QuickLoot's
`LootMenu` does not interrupt the remaining visual transaction. Startup and
save-load recovery also remove stale copies.

At release, the plugin revalidates the captured target, target type, activation
block, learned unlock spell, lock state, and Magicka. It then calls that
target's bound `REQ_LockpickControl.MagicUnlock` function and charges the real
spell's calculated cost. Requiem remains responsible for the unlock shader,
sound, crime handling, and object behavior. When that function completes for a
door, an SKSE game-thread callback snaps the door model closed while leaving
its lock open. The next activation uses Skyrim's normal opening animation,
sound, or cell transition.

A menu opened synchronously by Requiem's unlock is deferred until dispatch
returns, preventing a menu callback from invalidating the live transaction.
There are no worker threads. If the graph does not enter `IsShouting` within
500 milliseconds, or exits before release, a clearly logged captured-target
fallback still attempts the unlock.

## Requirements and installation

This build supports Skyrim 1.6.1170 and expects the LoreRim versions of
Requiem, `Requiem - Magic Redone.esp`, SkyUI, MCM Helper, Open Animation
Replacer, and Spell Hotbar 2. `ArcaneActivation.esp` has no Spell Hotbar
plugin master, but the
green art record uses Spell Hotbar's loose
`meshes/SpellHotbar/paralyzemasshandeffects_l.nif` asset.

Install the archive as an MO2 mod and enable `ArcaneActivation.esp`. The ESP is
ESL-flagged. Keep the packaged `meshes` directory and the DLL/INI under
`Data/SKSE/Plugins`. Nemesis does not need to be rerun: the replacement
animations are loose OAR assets and do not modify a behavior graph.

The SkyUI Mod Configuration Menu has two live settings:

- `Unlock delay`: delay before the unlock, from `0` through `1000` milliseconds
  in 25 millisecond steps. The default is `50`. Zero unlocks on the next update
  after `ShoutStart`. The animation uses a separate release deadline of at least
  450 milliseconds and continues through QuickLoot.
- `Show notifications`: shows the spell used and casting failure messages.

Changes take effect immediately and persist to
`Data/SKSE/Plugins/ArcaneActivation.ini`. The INI remains available for manual
configuration when the game is not running.

## Test checklist

1. Activate a locked chest in first person with weapons sheathed. Confirm a
   green left-hand cast plays before the chest unlocks and neither hand's
   equipment changes.
2. Repeat in third person. The aimed cast should play without equipping the
   unlock spell.
3. Set Unlock delay to zero. Confirm the chest unlocks immediately while the
   cast continues and reaches its normal release motion.
4. Draw a weapon and repeat. The hand art may be hidden by Skyrim, but a green
   effect should appear on the chest. Equipment should remain unchanged.
5. Unlock a regular interior door. Confirm the first press casts and unlocks
   without opening it, then activate the unlocked door again. It should use its
   normal opening animation and sound.
6. Unlock a load door. Confirm the first press casts and unlocks without moving
   or leaving the door half-open, then activate the unlocked door again to
   enter.
7. Try a key-required door and an activation-blocked quest door. Confirm Arcane
   Activation leaves both alone.
8. Spend enough Magicka that the unlock spell cannot be cast, then activate a
   compatible lock. Confirm the game reports insufficient Magicka and does not
   open the lockpicking menu.
9. Open the skills or pause menu during the charge, then leave it. The cast
   should clean up without a crash or a spell remaining on the player.
10. Check
   `Documents/My Games/Skyrim Special Edition/SKSE/ArcaneActivation.log`. A
   normal run should contain `ARCANE_ANIMATION_INTEGRATION` with both readiness
   values true, `ARCANE_FX_RESOURCE_PROBE` with `available=true`,
   `CAST_MARKER_ADD`, `CAST_FX_ADD`, `CAST_VISUAL_ROUTE`,
   `ARCANE_ANIMATION_START`, `ARCANE_ANIMATION_OBSERVED`,
   `ARCANE_ANIMATION_RELEASE`, `CAST_COMMITTED`, `CAST_FX_REMOVE`,
   `CAST_MARKER_REMOVE`, and `CAST_CLEANUP_COMPLETE`.
11. The startup block should contain four successful `ARCANE_RESOURCE_PROBE`
   entries: the parent OAR config, submod config, and one first- and
   third-person animation.
12. If the animation is missing, include the full block from `CAST_BEGIN`
   through `CAST_CLEANUP_COMPLETE`. It records the graph result, marker/FX
   ownership, camera and weapon state, animation events, and every animation
   notification result. Door casts also record `target_kind=door` or
   `target_kind=load_door`. A successful door correction records
   `DOOR_UNLOCK_CALLBACK` followed by `DOOR_RECLOSED`, including the captured
   and current door classifications.

## Build

The developer-only ESP generator copies Spell Hotbar 2's casting-effect layout
and left-hand Alteration art record, then rewrites all form references and
validates that the result has no Spell Hotbar plugin master. The installed
Spell Hotbar loose green-hand mesh remains a runtime asset dependency.

The build requires Windows, Visual Studio 2022 with C++ support, CMake 3.24 or
newer, vcpkg, Python, Windows PowerShell, and a CommonLibSSE-NG checkout. Set
`VCPKG_ROOT` and `ARCANE_ACTIVATION_COMMONLIBSSE_NG_DIR` before configuring.
The Papyrus build script accepts `-LoreRimRoot` and `-ScriptsArchive` when the
default LoreRim and Steam locations do not apply. The ESP generator accepts
`-XEdit` or reads `SSEEDIT_EXE`; otherwise it searches `PATH` for
`SSEEdit64.exe`.

```powershell
cmake --preset ae
cmake --build --preset release
```

The build runs `tools/validate_oar_package.py` before packaging. It rejects a
missing parent manifest, unresolved presets, an invalid marker-spell condition,
or mismatched first- and third-person animation sets. Run it directly with:

```powershell
python tools\validate_oar_package.py package\meshes\actors\character\OpenAnimationReplacer\ArcaneActivation
```

## Animation change gate

Use this order for animation work. Do not debug later layers while an earlier
gate is unproven.

1. **Package:** Run the OAR validator. The parent manifest, presets, submod
   manifest, conditions, and both camera animation sets must pass.
2. **Registration:** Start the game once and confirm OAR's text log lists
   `Arcane Activation` and `Arcane Activation left-hand cast`. If it does not,
   stop and inspect the OAR directory hierarchy and JSON before changing C++.
3. **Selection:** Trigger the action and confirm the OAR log replaces the
   expected shout filenames instead of reporting their original providers.
4. **Graph:** Only after selection is proven, inspect `ShoutStart`,
   `IsShouting`, release events, and animation-event timing in
   `ArcaneActivation.log`.
5. **Presentation:** Evaluate first- and third-person motion, visual effects,
   timing, and interruption cleanup after registration, selection, and graph
   state have all been demonstrated.

The source package is the deployment authority. Build `package_release`,
validate the staged copy, deploy that staging directory, then compare the live
DLL, ESP, and parent OAR config hashes with staging before launching Skyrim.

The current log is `ArcaneActivation.log`. The logger keeps the three previous
launches as numbered rotated logs.

The release archive is `build/release/ArcaneActivation-1.2.1.zip`.
