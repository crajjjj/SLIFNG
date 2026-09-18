# MCM Reference

Two pages. Settings is everything you can change; Actor is read-only diagnostics.

## Settings

### Diagnostics column

| Option | Meaning |
|---|---|
| **Engine API version** | The native surface version other mods compile against. |
| **RaceMenu / skee** | `OK` when both skee interfaces were found; `morphs only` means node scaling is unavailable; `NOT FOUND` means RaceMenu is missing or too old. |
| **Tracked actors** | How many actors currently have inflation state in the ledger. |
| **Verbose logging** | Logs every API call and every apply with a RaceMenu readback. Great for diagnosis, noisy for play. |
| **Dump to SLIFNG.log** | Writes the whole ledger - every actor, every contribution, every fold result - to the log. |

### Behaviour column

#### Calculation type

When **several mods push the same target at once**, this decides what one value wins. These are old SLIF's own six types, its numbering, its default. Example with three mods on one belly (3.0, 2.2, 1.5):

| Type | Result | Reading |
|---|---|---|
| **Top X** (default) | 3.0 + 2.2/3 + 1.5/6 = **3.98** | The biggest leads, the others add dampened fractions |
| Highest wins | **3.0** | Only the largest shows - a pregnant character who overeats stays pregnancy-sized |
| Subtract and add one | 1 + 2.0 + 1.2 + 0.5 = **4.7** | Deviations from neutral stack |
| Square root | sqrt(9 + 4.84 + 2.25) = **4.01** | Softened stacking |
| Average | **2.23** | Middle ground |
| Additive | **6.7** | Everything stacks fully |

It applies across mods to nodes and to sliders alike; one mod's own node and morph layers still add (they are that mod's single intent). Selection opens a menu dialog; a change recomputes and re-applies every tracked actor immediately.

#### Incremental inflation

On (default): bodies step toward new sizes - each mod's own increment, 0.1 by default, per quarter second - instead of snapping. Runs natively, costs the script engine nothing, pauses while the game is paused. Hiding a node (chastity belts) and unregistering stay instant. Off: everything applies instantly, old SLIF's shipped behaviour.

#### Overall magnitude

One multiplier over everything SLIF NG applies. `1.00x` shows exactly what mods intended; `0.00x` suppresses all inflation. Applies immediately, retroactively, and never changes what mods have stored - it is a display knob, not a data edit. (Per-target and per-actor multipliers exist in the [API](../authors/query-api.md) for finer control; they are deliberately not a page of MCM sliders.)

### Migration column

**Old-SLIF import** is a status row, not a button: the import runs by itself through the MCM version update on the first load of a save that ran old SLIF. `done (automatic)` / `nothing to import` are the normal states; `pending` only appears if the automatic pass could not run, and then the row is clickable as a manual fallback.

## Actor

A snapshot of one actor - the player, or whatever is under your crosshair (toggle with **Showing**, then close and reopen the menu while looking at someone).

| Section | What it tells you |
|---|---|
| **Actor** | Name, FormID, race, sex. |
| **Body** | The resolved body profile and how many standard skeleton nodes were found (missing ones are listed - a missing `NPC Belly` means no belly node scaling on that skeleton). |
| **Contributions** | One block per inflation target, one row per mod driving it, decrypted: `Node Belly (NPC Belly)` with `> drives PregnancyBelly 0.133 / +1.0` shows what the node becomes on this body. |
| **Applied** | The final value per slider and per node after folding and magnitude. `skee disagrees` rows mean RaceMenu holds something else than we wrote (another mod interfered). `also <key>` rows list **foreign RaceMenu keys** stacking on the same slider - the usual culprit when a body is bigger than the numbers above explain. |
| **Aggregation** | The active calculation type. |

**Reset this actor** wipes everything SLIF NG stores for the shown actor and clears the applied inflation, behind a confirmation. Mods may or may not re-send their values afterwards - some push every game tick, others only on events (a meal, a scene, a pregnancy update) - so use it to clear stuck state, not as an undo.
