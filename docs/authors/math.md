# Aggregation Math

The arithmetic is old SLIF's, kept formula for formula - it was the field-tested part of the reference. This page is the complete pipeline from a consumer's call to a skee write.

## Per contribution

Each `(actor, mod, target)` row stores `value, min, max, mult, increment`. Its effective value is:

```
effective = clamp(value, min, max) * mult          ; inverted bounds are swapped, not UB
```

Defaults when a consumer passes `-1.0`: `min 0`, `max 100`, `mult 1.0`, `increment 0.1`.

## Node targets: the six calculation types

When several **mods** hold rows on one node target, `addCalculationType` folds their effective values (non-positive contributions are skipped; every type except subtract-one falls back to the neutral `1.0` on an empty or non-positive result; a lone below-neutral value CAN show):

| # | Type | Fold over the sorted (desc) positive contributions |
|---|---|---|
| 0 | **Top X** (default) | `v0 + v1/3 + v2/6` (top_x = 3; further places divide by `3*x`) |
| 1 | Highest wins | `v0` |
| 2 | Subtract and add one | `1 + SUM(v_i - 1)`, floored at 0 |
| 3 | Square root | `sqrt(SUM(v_i^2))` |
| 4 | Average | plain average |
| 5 | Additive | plain sum |

There is **no post-fold clamp** - the per-contribution bounds are the only clamp, exactly as in the reference.

A **hidden** node (`hideNode`) overrides the fold entirely: the pin value shows until `showNode`, whatever anyone contributes.

## From node to sliders: the body profile

If the actor's body profile maps a node target to sliders, the node value is transformed:

```
sliderShare = (nodeValue - 1.0) * (MorphMax / FullScale)
```

per listed slider (see [Body Profile Format](body-profiles.md)). A target the profile does not list drives the skeleton bone directly instead.

## Slider targets: one value per mod, then the same fold

For each BodySlide slider, every mod gets **one combined statement**:

```
perMod = (its direct morph contribution, raw)  +  (its node rows transformed through the profile)
```

Within a mod the two layers ADD - a mod sending `slif_belly` *and* `PregnancyBelly` means them together. ACROSS mods, the calculation type folds the per-mod values exactly as it folds nodes (in slider space neutral is `0.0`; zeros drop out; subtract-one degenerates to the plain sum).

!!! note "Deliberate deviation from the reference"
    Old SLIF summed morph contributions unconditionally - but that corner shipped disabled (every stock morph percent is 0) and was never field-tested, and once a profile *transforms* a node into a slider, "several mods, one physical target" applies to the slider. Composition must not depend on which API spelling a mod used (Fill Her Up sends the same belly as `"NPC Belly"` or as a morph depending on its own MCM).

**Worked example** (a real migrated save): Beeing Female `PregnancyBelly 0.495` direct, Fill Her Up `0.090` direct, Sexlab Survival `slif_belly 2.2` transformed through the 3BA profile to `0.160`:

| Calc type | Applied PregnancyBelly |
|---|---|
| Highest wins | `0.495` (Beeing Female alone; the others are masked) |
| Top X | `0.495 + 0.160/3 + 0.090/6 = 0.568` |
| Additive | `0.745` |

The direct sum alone (`0.585`) is still bookkept and mirrored to StorageUtil as `slif_PregnancyBelly` - that is reference bookkeeping, not the applied value.

## Magnitude, then skee

The final write multiplies the fold by the user's magnitude - `master * perTarget * perActor` - applied to the *deviation* for node scales (so `1.0` stays neutral) and to the value for sliders. Exactly `0.0` clears a morph; a node scale within epsilon of `1.0` removes the transform.

## Incremental inflation

With the ramp on, a changed target does not snap: its *shown* value travels toward the fold at the triggering row's `increment` **per quarter second**, retargeting live if the fold changes mid-ramp. That rate is the contract; the tick cadence is an independent implementation detail (100 ms, so each tick moves 0.4 of the increment). Shortening the cadence buys smoothness without changing speed, and the MCM's *Inflation speed* multiplier is the only thing that changes speed. Node ramps drag their derived sliders along proportionally; direct-morph targets ramp in slider space. Hide, unregister and direct morphs' bookkeeping stay instant; ramps pause with the game and snap to the fold on save load or actor unload. One coalesced apply per actor per tick.
