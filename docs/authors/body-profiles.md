# Body Profile Format

Profiles live in `Data/SLIFNG/Bodies/*.ini`. They answer the one question skee cannot: *which BodySlide sliders does this actor's body actually have?* (skee's morph store accepts any name and silently drops unknown ones at apply time, so the only honest source is a file that says so.)

## Resolution, per actor

1. Every `.ini` in the folder loads at game start (alphabetical). `default.ini` is the fallback - the installer writes your body choice there.
2. A profile may carry matchers, and the two are **different kinds of evidence**:
    - `Race=` — a substring of the race EditorID. **Per-actor**: it says *this actor uses this body*. This is how UBE characters get UBE sliders inside a 3BA game.
    - `Plugin=` — a plugin's presence. **Install-wide**: it says *this body is installed somewhere*, which says nothing about any individual actor.
3. Race wins. A profile that declares `Race=` and does **not** match this actor is out of the running — its `Plugin=` cannot rescue it, and in fact is never consulted at all. Plugin presence only ever selects a profile that offers no `Race=`. Across profiles the same order holds: every race match is considered before any plugin match, then `default.ini`; with no files at all, the built-in fallback is **node scaling only** — old SLIF's own out-of-the-box behaviour.

    !!! warning "Do not declare both"
        A profile with `Race=` and `Plugin=` logs a warning at load: the plugin line is dead. Before 0.4.8 it was worse than dead — it made the profile claim *every* actor once its plugin was present, which is [issue #1](https://github.com/crajjjj/SLIFNG/issues/1): shipped `UBE.ini` carried both, so a load order with UBE resolved every Nord and Imperial to `UBE 2.0`.

Reloading: edit the INI, then `cgf "SLIFNG_Debug.Body"`-style reload is not needed - just reload a save (profiles are read at data load; the MCM actor page shows what each actor resolved to).

## Schema

```ini
[Profile]
Name=CBBE 3BA
;Race=UBE_            ; per-actor matcher, decisive - see above
;Plugin=SomeBody.esp  ; install-wide fallback; use INSTEAD of Race=, never with it

[slif_belly]
FullScale=7.5         ; the node-scale DEVIATION at which sliders reach their Max
Morph1=PregnancyBelly ; EXACT case, as in the body's .osp <Slider name="...">
Morph1Max=1.0         ; slider value at FullScale
[slif_breast]
FullScale=10.0
Morph1=BreastsNewSH
Morph1Max=0.4
Morph2=DoubleMelon
Morph2Max=0.25
Morph3=BreastsSmall
Morph3Max=-0.2        ; negative = the slider runs in reverse
```

The applied share per slider is `(nodeValue - 1.0) * MorphMax / FullScale`, so a neutral actor gets exactly zero. Up to 16 `MorphN` entries per section; numbering must not skip.

**A section you do not write means "this body cannot morph that"** and the target drives its skeleton bone instead - that is the supported way to say it, and it is why the node fallback is reachable at all. `slif_butt` and `slif_scrotum` ship unmapped on every profile for exactly this reason.

## Custom regions

Any section whose name is **not** a canonical `slif_*` key defines a *semantic region*, addressable by consumers as the key `region:<section>`:

```ini
[Weight]
FullScale=2.0
Morph1=ChubbyWaist
Morph1Max=0.6
Morph2=ChubbyButt
Morph2Max=0.4
```

A consumer then sends intent, not slider names:

```papyrus
SLIF_Main.inflate(akActor, "My Mod", "region:weight", 1.4)
```

Node-scale semantics (`1.0` = neutral, folds across mods like any target), profile sliders only - there is no skeleton bone behind a region. On a profile without the section it is a logged no-op. This is the mechanism that lets a consumer (SGO4 being the intended first adopter) drop its own per-body FOMOD entirely: the profile decides sliders per body, the mod just says how much.

A commented `[Weight]` template ships in the CBBE 3BA profile.

Because a region can silently do nothing, a consumer should ask first and keep a fallback:

```papyrus
if SLIFNG.HasTarget(akActor, "region:weight")
    SLIF_Main.inflate(akActor, "My Mod", "region:weight", 1.4)
else
    SLIF_Main.inflate(akActor, "My Mod", "slif_belly", 1.15)   ; bone fallback
endif
```

See [`HasTarget`](query-api.md#hastarget-ask-before-you-write) (needs `SLIFNG.GetVersion() >= 3`).

## Region overlays

A profile is a whole file and only one wins per actor, so a consumer mod cannot add a region to it without overwriting the user's body choice. **Overlays** solve that: every `.ini` in `Data/SLIFNG/Bodies/Regions/` has its sections merged into the profiles it names, at load, without touching them.

```ini
; Data/SLIFNG/Bodies/Regions/SGO4-3BA.ini
[Overlay]
Profile=CBBE 3BA        ; matches a profile's Name=, or * for every body

[MuscleMass]
FullScale=2.0
Morph1=MuscleDefinition
Morph1Max=0.7
```

Same schema as a profile, plus `[Overlay] Profile=`. It matches on the profile's **`Name=`**, not on race or plugin, because a region is a set of sliders and sliders are a property of the *body* - and `Name=` is the only thing that identifies which body a profile describes, whether that profile is `default.ini` or a matcher-selected file like `UBE.ini`.

Two audiences:

- **Mod authors.** Ship one small file per body you know (`MyMod-3BA.ini`, `MyMod-UBE.ini`), each naming its own `Profile=`. Name files after your mod so they can never collide - overlays are separate files, so any number of mods coexist.
- **Users.** Correct or tune a slider for your own body without editing a shipped profile that the next update overwrites.

Rules:

- A whole **section** is the unit: an overlay section *replaces* the profile's. That is what makes fixing a wrong shipped slider name possible.
- Overlays apply in **alphabetical order, last wins**. Name yours `zz-*.ini` to beat everything else. A collision between two overlays is written to `SLIFNG.log`, never silent.
- They can redefine canonical keys (`[slif_belly]`) too, not just custom regions.
- Merging happens once at load, so there is no per-actor cost.

`SLIFNG.log` records every overlay it loaded and which profiles it merged into; the folder ships a `README.txt` with the same reference.

## Shipped profiles

| File | Matcher | Notes |
|---|---|---|
| `default.ini` | none (installer's choice) | CBBE 3BA / CBBE / BHUNP variant, or absent for node-only |
| `UBE.ini` | `Race=UBE_` + `Plugin=UBE_AllRace.esp` | Always installed; slider names verified against the UBE 2.0 .osp, including the literal `" n|p"` suffix |

Slider names in the CBBE 3BA and UBE profiles are verified against their reference `.osp` files; BHUNP's are sourced from old SLIF's own UUNP table and unverified against a live body - if a slider does not exist in your `morphs.tri`, skee silently drops it, so wrong names degrade to "nothing happens", never to errors.
