# Arcane Activation

Arcane Activation lets a mage use Requiem - Magic Redone's Alteration unlock
spells by activating a locked container. It chooses the cheapest learned spell
that can open the lock, checks Magicka, plays a dedicated first- or third-person
cast, and asks Requiem to perform the unlock.

The unlock delay is configurable from 0 to 1000 milliseconds. Animation timing
is separate, so an instant unlock can still finish its casting motion. QuickLoot
does not cancel the remaining animation transaction.

See [the full documentation](docs/ArcaneActivation.md) for requirements,
installation, architecture, logs, and the in-game test checklist.

## Build

```powershell
cmake --preset ae
cmake --build --preset release
```

The release archive is written to
`build/release/ArcaneActivation-1.2.0.zip`.
