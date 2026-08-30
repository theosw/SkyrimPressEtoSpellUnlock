# Arcane Activation

Arcane Activation lets a mage use Requiem - Magic Redone's Alteration unlock
spells by activating a locked container or door. It chooses the cheapest
learned spell that can open the lock, checks Magicka, plays a dedicated first-
or third-person cast, and asks Requiem to perform the unlock.

The unlock delay is configurable from 0 to 1000 milliseconds. Animation timing
is separate, so an instant unlock can still finish its casting motion. QuickLoot
does not cancel the remaining animation transaction.

Load doors remain physically closed after the spell unlocks them. Activate the
unlocked door again to enter. A valid spell attempt with too little Magicka is
consumed and reports the shortage instead of falling through to lockpicking.

See [the full documentation](docs/ArcaneActivation.md) for requirements,
installation, architecture, logs, and the in-game test checklist.

## Build

The build requires Windows, Visual Studio 2022 with C++ support, CMake 3.24 or
newer, vcpkg, Python, Windows PowerShell, and a CommonLibSSE-NG checkout. Set
these environment variables before configuring:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
$env:ARCANE_ACTIVATION_COMMONLIBSSE_NG_DIR = "C:\path\to\CommonLibSSE-NG"
```

The release target also compiles the Papyrus scripts. Its defaults expect a
LoreRim installation at `D:\Lorerim` and Skyrim's `Scripts.zip` in the standard
Steam directory. Pass `-LoreRimRoot` and `-ScriptsArchive` directly to
`tools\Build-ArcaneActivationMCM.ps1` when those paths differ.

```powershell
cmake --preset ae
cmake --build --preset release
```

The release archive is written to
`build/release/ArcaneActivation-1.2.1.zip`.
