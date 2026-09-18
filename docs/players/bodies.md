# Bodies, Nodes & Morphs

Two completely different mechanisms can change a body's shape, and SLIF NG uses both. Knowing the difference explains almost everything you see on the MCM's actor page.

## Node vs. morph

| | **Node (bone scale)** | **Morph (BodySlide slider)** |
|---|---|---|
| What it is | A skeleton bone in the loaded 3D: `NPC Belly`, `NPC L Breast`... (from XPMSSE) | A named set of per-vertex offsets baked into the mesh: `PregnancyBelly`, `BreastsNewSH`... |
| What changing it does | Multiplies the bone's scale; everything skinned to that bone balloons uniformly around it | Slides vertices along artist-sculpted offsets toward a designed shape |
| Neutral value | `1.0` (a multiplier) | `0.0` (an additive weight) |
| Works on | **Any** body and any equipped outfit - the bone exists regardless of the mesh | **Only** meshes built in BodySlide with *Build Morphs* checked, and only if that mesh has that exact slider |
| Body dependence | None - `NPC Belly` is the same bone on CBBE, BHUNP and UBE | Total - slider names differ per body family (`BreastsNewSH` vs `BreastsSSH` vs `BreastsBigger`) |
| Look | A uniform stretch: robust, but "beach ball" at large sizes | Sculpted: a pregnancy belly actually shaped like one |

**Node scaling is robust and ugly at the extremes; morphs are beautiful and fragile about prerequisites.** Old SLIF ships node-only because it cannot know which sliders your body has. SLIF NG keeps that as the safe fallback and moves the knowledge into the installer - the one moment you actually know which body you built.

!!! note "Outfits"
    A worn outfit only follows a *morph* if the outfit was also built with morphs. A *node* scale moves the bone, so any outfit follows automatically. That is why node scaling remains a first-class path rather than legacy cruft.

## The node targets

There are four, and the list is fixed: they are what old SLIF exposed, so they stay frozen for compatibility. SLIF NG ships no skeleton of its own - it scales bones **XPMSSE** already provides, and a bone your skeleton happens to lack is a silent no-op (the MCM's `Body / Skeleton nodes` row lists the missing ones).

| Key | Shown on the actor page | Skeleton bone(s) |
|---|---|---|
| `slif_belly` | `Node Belly (NPC Belly)` | `NPC Belly` |
| `slif_breast` | `Node Breasts (L+R)` | `NPC L Breast` + `NPC R Breast` |
| `slif_butt` | `Node Butt (L+R)` | `NPC L Butt` + `NPC R Butt` |
| `slif_scrotum` | `Node Scrotum` | `NPC GenitalsScrotum [GenScrot]` |

`slif_breast` and `slif_butt` are **sync pairs**: both bones always receive the same value. The side aliases `slif_left_breast`, `slif_right_breast`, `slif_left_butt` and `slif_right_butt` resolve to the pair, not to one side.

!!! note "The list is closed"
    A mod cannot teach SLIF NG a fifth bone. Anything outside this table and its aliases is rejected with a log line, and [`HasTarget`](../authors/query-api.md#hastarget-ask-before-you-write) answers `false` for it. Old SLIF differed: it used any unrecognised string as a bone name, so it would scale whatever you named. Extension happens on the **morph** side instead, where it *is* data-driven - `morph:<slider>` for a named BodySlide slider, or a `region:<name>` that the body profile maps to sliders.

## Body profiles

Your installer choice lands as `Data/SLIFNG/Bodies/default.ini` - a small INI that says, per inflation target, which sliders this body drives and how strongly. A target the profile does not list falls back to the skeleton node.

Profiles resolve **per actor**, not per game:

1. **Race match first** - UBE 2.0 ships its own races, so UBE characters are detected reliably and get UBE's slider names even in a 3BA load order.
2. **Your installer choice** covers everyone else.

The MCM actor page shows which profile an actor resolved to (`Body / Profile`), how many of the standard skeleton nodes were found, and - per inflation source - what each node key *becomes* on this body (`> drives PregnancyBelly` or `> drives bone scale`).

## How mods talk to this

Mods send SLIF NG either a **node key** (`slif_belly`, or the raw bone name `NPC Belly` - both mean the same target) or a **direct morph** by slider name (`PregnancyBelly`). The actor page decrypts both: node sections read `Node Belly (NPC Belly)`, morph sections `Morph PregnancyBelly`.

When a node key lands on a body whose profile maps it to sliders, the node value is *transformed* into those sliders - and from that point it competes with direct morph contributions on the same slider under the selected [calculation type](mcm.md#calculation-type). The full arithmetic lives in [Aggregation Math](../authors/math.md).

Profiles can also define **custom regions** (for example `[Weight]`) that integrating mods may drive as a single intent; see [Body Profile Format](../authors/body-profiles.md) if you want to enable the shipped template.

## Changing a profile without losing it on update

Shipped profiles are replaced when you update SLIF NG, so edits made directly in `default.ini` do not survive. Put your changes in an **overlay** instead: any `.ini` in `Data/SLIFNG/Bodies/Regions/` has its sections merged into the profile it names, and nothing overwrites it.

```ini
; Data/SLIFNG/Bodies/Regions/zz-my-fixes.ini
[Overlay]
Profile=CBBE 3BA        ; the Name= at the top of the profile you are fixing

[slif_breast]
FullScale=10.0
Morph1=TheSliderMyBodyActuallyHas
Morph1Max=0.5
```

A whole section replaces the profile's, so this is how you correct a slider name that does not exist on your body (the BHUNP profile's names are unverified, so this is the fix if breasts never move). Files apply alphabetically and the last wins, so name yours `zz-*.ini` to beat everything else. `SLIFNG.log` lists every overlay it loaded and which profile it merged into.

Mod authors use the same mechanism to add regions; the folder ships a `README.txt`, and the full rules are in [Body Profile Format](../authors/body-profiles.md#region-overlays).
