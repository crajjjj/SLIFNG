# Overview & Architecture

## The one-paragraph version

Consumer mods call the same Papyrus surface old SLIF exposed (`SLIF_Main`, `SLIF_Morph`, three mod events). Those scripts are thin shims over native functions in `SLIFNG.dll`. The DLL keeps a **ledger** - per actor, per mod, per target: value, min, max, mult, increment - in the SKSE co-save, folds contributions with SLIF's own calculation types, transforms node values into BodySlide sliders through the actor's **body profile**, and pushes results to RaceMenu's skee interfaces with one coalesced apply per actor. Applied skee values are derived output: every game load recomputes them from the ledger, so the system self-heals by construction.

```
consumer .pex ──(mod events)──> SLIF_ScannerAlias.psc ─┐
consumer .pex ──(direct calls)─> SLIF_Main / SLIF_Morph ┴─> SLIFNG.psc (natives)
                                                              │
                                                        SLIFNG.dll
                                    ┌─────────────────────────┼──────────────────────┐
                                 Ledger                    BodyProfile             Skee
                        contributions, folds,          per-actor INI          NiTransform +
                        hidden pins, magnitudes,       profiles, regions      BodyMorph writes,
                        ramp display overrides                                one apply per batch
                                    │
                              SKSE co-save ('SLIF', versioned, ResolveFormID-safe)
```

## Key design rules

- **The ledger is the source of truth.** Skee output is never read back to make decisions; on load, everything is recomputed and re-applied (`kPostLoadGame`), and actors without loaded 3D are retried when they stream in.
- **Targets are canonical.** `slif_belly`, the raw node name `NPC Belly`, and Fill Her Up's event spelling all resolve to one ledger target, so mods aggregate instead of clobbering. Morph targets are `morph:<slider>`; custom regions are `region:<name>`.
- **One skee key.** All output lands under `"SexLab Inflation Framework.esp"` - old SLIF's own key - which is what makes migrated saves' stale values overwritable in place, and unregistration a one-key cleanup.
- **Thread discipline.** Papyrus natives run on VM worker threads; all skee geometry work is posted to the main thread via the task interface. Batches coalesce to one `ApplyBodyMorphs` per actor.
- **Loud failure.** Unknown keys, dead keys, missing profile sections and unimplemented API calls each produce one searchable log line instead of silence.

## Script inventory

| Script | Role |
|---|---|
| `SLIF_Main.psc`, `SLIF_Morph.psc` | The pinned SLIF-compatible surface (frozen signatures) |
| `SLIF_ScannerAlias.psc` | The three mod events, on the player alias of quest `0x800` |
| `SLIF_Menu.psc` | The MCM, including the version-update ladder that triggers migration |
| `SLIFNG.psc` | The native surface the shims call - also the author-facing extension API |
| `SLIFNG_Migrate.psc` | The one-shot StorageUtil walk for old-SLIF saves |
| `SLIFNG_Version.psc` | The single source of the packed mod version |
| `SLIFNG_Debug.psc` | `cgf` console drivers |
| `SLIF_Scanner.psc`, `SLIF_Timer.psc` | Stubs that keep SkyUI's config manager from aborting on a migrated save |

## Where to go next

- [Write API](write-api.md) - everything that changes state
- [Query API](query-api.md) - everything that reads it, from Papyrus or C++
- [Aggregation Math](math.md) - exactly how contributions become one value
- [Body Profile Format](body-profiles.md) - the INI schema and custom regions
- The full pinned contract, with provenance and the bug-for-bug quirks: [CONTRACT.md](https://github.com/crajjjj/SLIFNG/blob/main/CONTRACT.md)
