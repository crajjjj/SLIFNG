# Supported Mods

SLIF NG's compatibility surface was pinned by decompiling old SLIF 1.2.2 and every consumer in a large live load order - the implemented API is exactly what installed mods actually call, verified in bytecode. If a mod worked against SLIF, it works here, unpatched.

## Surveyed consumers

| Mod | How it drives SLIF NG | Notes |
|---|---|---|
| **Beeing Female NG** | `inflate` (`slif_belly`, `slif_breast`) + direct morphs, legacy key `BeeingFemale` cleaned on first contact | Pregnancy belly/breast growth and reset |
| **Fill Her Up (Baka)** | The `SLIF_inflate` event with the raw node name `"NPC Belly"`, direct `PregnancyBelly` morphs, `unregisterNode`; legacy keys `FHU_MODKEY` / `sr_FillHerUp.esp` cleaned | Both of its spellings land on the same target, so it aggregates with everything else instead of clobbering |
| **Sexlab Survival** | The `SLIF_inflate` event (`slif_belly`), `unregisterActor`, plus a READ path: `GetValue("All Mods", ...)` every game hour and a direct StorageUtil read of `slif_<morphName>` | Both reads are served; a whole scene branch gates on them |
| **Estrus Chaurus (core)** | `GetValue` / `GetMinValue` / `GetMaxValue` (including the `slif_left_breast`-style side keys), `inflateBoth`, `resetActor`, `updateActorList` | Its bound reads clamp its own growth - absent rows return the caller's default, exactly as the reference did |
| **Estrus Chaurus Spider Addon** | The `SLIF_inflate` event, including the dead keys `slif_breast01` / `slif_breast_p` | Dead keys stay silent no-ops (bug-compatible), but the log reports them |
| **Devious Devices NG** | `hideNode` / `showNode` with raw node names | Belly pinned flat under a chastity belt, restored on unequip; the hide wins over every other mod until released |
| **Devious Interests** | `SLIF_Morph.unregisterMorph` | Drops only its own contribution; other mods' morphs on the same slider survive |
| **Milk Mod Economy** | `unregisterActor`, without ever registering | A safe no-op that still clears stale old-SLIF output - which is exactly why MME calls it |

## Unsurveyed mods

Any SLIF consumer not listed above will either work (if it uses the same calls) or fail **loudly**: a call into a function SLIF NG does not implement produces one searchable line in the Papyrus log:

```
error: <fn> is not a function or does not exist
```

That line is the detection mechanism, by design. If you hit one, report the mod name and that line - implementing a genuinely used entry point is cheap once it is known to be used.

## Old-SLIF saves

Migration is automatic on first load: the old framework's per-actor, per-mod values are read out of StorageUtil and rebuilt in the native ledger, and stale applied values are overwritten in place (SLIF NG deliberately uses old SLIF's own RaceMenu key). See [Getting Started](getting-started.md).
