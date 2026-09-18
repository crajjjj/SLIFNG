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
