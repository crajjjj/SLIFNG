Native SKSE rewrite (CommonLibSSE-NG) of SexLab Inflation Framework. Install it INSTEAD of SLIF: same plugin name, same API, same math, so Beeing Female NG, Fill Her Up, Sexlab Survival, Estrus Chaurus, Devious Devices and friends keep working unpatched. Old SLIF saves migrate themselves on first load. Zero configuration - pick your body in the installer and play. Runs on SE, AE & VR.



[center][size=5][b]🎈 SLIF NG 🎈[/b][/size]
[size=3]SexLab Inflation Framework, rebuilt as an SKSE plugin[/size][/center]

[b]SLIF NG[/b] is a modern, zero-configuration replacement for [b]SexLab Inflation Framework SE[/b] by [i]qotsafan[/i].

[b]What a body inflation framework is for:[/b] plenty of mods want to change the shape of a body - a pregnancy belly, cum inflation, milk-filled breasts, weight from overeating - and on their own they all reach for the same belly bone or the same BodySlide slider and overwrite each other, so whichever ran last wins and the rest quietly vanish. SLIF is the middleman they hand their requests to instead: it keeps every mod's contribution separately, combines them by a rule you choose, and applies one final shape to the actor. Installing it does nothing by itself - it is the plumbing the mods below use.

You install it [b]instead of[/b] SLIF. Every mod that talks to SLIF today keeps working without any patch, and a save that ran the old framework migrates by itself on first load. No JSON to edit, no nine pages of sliders to understand.

Under the hood the Papyrus framework is gone. A native SKSE plugin ([b]SLIFNG.dll[/b], one DLL for SE / AE / VR) keeps every mod's inflation values in the co-save and applies them through RaceMenu, while thin script shims keep the exact API old SLIF exposed: same function names, same arguments, [b]same math[/b].

[b]Supported versions:[/b] Skyrim SE • AE • VR



[size=4][b]❓ TL;DR - Why replace SLIF?[/b][/size]

The old framework is ~11,700 lines of Papyrus and ~90 public functions, but installed mods only ever call a small fraction of it. What players meet is the other side: a framework that ships inert - all 270 sliders across its three body tables sit at 0%, so a stock install scales skeleton bones and drives no BodySlide morph at all - plus a nine-page MCM of body-specific JSON editing, and silent failure when any of it is misconfigured.

[list]
[*][b]Native engine[/b] - the aggregation and the ramp run in C++, not on the script engine[/*]
[*][b]Zero configuration[/b] - one installer question instead of nine MCM pages[/*]
[*][b]No patches needed[/b] - the API is pinned from SLIF 1.2.2 bytecode, signature for signature[/*]
[*][b]Automatic migration[/b] - your characters keep their shape[/*]
[*][b]Diagnoses instead of failing silently[/b] - an actor page that shows who inflates what[/*]
[*][url=https://crajjjj.github.io/SLIFNG/]User guide[/url] !!! [color=#cc0000]Check before asking questions[/color][/*]
[/list]




[size=4][b]✨ Core Features[/b][/size]

[list]
[*][b]The same math, kept on purpose[/b]
[list]
[*]All six of SLIF's calculation types, its numbering, its default (Top X)[/*]
[*]Same clamps, multipliers and defaults, formula for formula[/*]
[*]The calculation type now applies to BodySlide sliders the same way it applies to nodes[/*]
[/list]
[/*]
[*][b]Bodies without homework[/b]
[list]
[*]Pick your body once in the FOMOD: CBBE 3BA, generic CBBE, BHUNP, or "none - node scaling" (works with any body, exactly like old SLIF)[/*]
[*]Verified slider names, no hand-edited JSON[/*]
[*]UBE 2.0 support installs automatically and matches by race, even alongside a 3BA game[/*]
[*]Per-actor body profiles, tunable through overlays that updates cannot overwrite[/*]
[/list]
[/*]
[*][b]Incremental inflation, for free[/b]
[list]
[*]Bodies swell toward new sizes in steps instead of snapping[/*]
[*]A native ramp, off the script engine entirely - on by default[/*]
[*]MCM speed slider from 0.10x to 5.00x, retiming even inflation already in progress[/*]
[/list]
[/*]
[*][b]Diagnostics instead of guesswork[/b]
[list]
[*]Per-actor page: who inflates what, what it becomes on your body, what RaceMenu actually shows[/*]
[*]Foreign RaceMenu keys stacking on the same slider are named - the usual culprit when a body is bigger than the numbers explain[/*]
[*]Full ledger dump to the log, verbose mode, per-actor reset[/*]
[/list]
[/*]
[*][b]Save-friendly state[/b]
[list]
[*]One compact native co-save record instead of hundreds of StorageUtil keys[/*]
[*]Recomputed and re-applied on every load, self-healing[/*]
[*]Automatic old-SLIF import, reported in a notification[/*]
[/list]
[/*]
[*][b]One page of settings[/b]
[list]
[*]Calculation type, incremental inflation, inflation speed, overall magnitude[/*]
[*]Overall magnitude is a display knob: it never edits what mods stored[/*]
[/list]
[/*]
[/list]



[size=4][b]🔗 Supported Mods[/b][/size]

Compatibility was pinned by decompiling old SLIF 1.2.2 and every consumer in a large live load order - the implemented API is exactly what installed mods actually call, verified in bytecode. No patches, no compatibility versions.

[list]
[*][b]Beeing Female NG[/b] - pregnancy belly/breast growth and reset[/*]
[*][b]Fill Her Up (Baka)[/b] - both of its spellings land on the same target, so it aggregates instead of clobbering[/*]
[*][b]Sexlab Survival[/b] - including its read path (a whole scene branch gates on it)[/*]
[*][b]Estrus Chaurus[/b] + [b]Spider Addon[/b] - bound reads, dead keys kept bug-compatible[/*]
[*][b]Devious Devices NG[/b] - belly pinned flat under a chastity belt, restored on unequip[/*]
[*][b]Devious Interests[/b] - drops only its own contribution[/*]
[*][b]Milk Mod Economy[/b] - its stale-state cleanup still works[/*]
[/list]

Any SLIF consumer not on that list either works (same calls) or fails [b]loudly[/b]: one searchable line in the Papyrus log. That line is the detection mechanism, by design - report it and the entry point is cheap to add.



[size=4][b]📋 Requirements[/b][/size]

[list]
[*][b]SKSE64[/b] (or SKSE VR)[/*]
[*][b]SkyUI[/b] - the MCM[/*]
[*][b]RaceMenu[/b] - its skee plugin performs the actual body changes. Both generations work: the AE build (0.4.19+) and pre-AE RaceMenu 0.4.16 on Skyrim SE 1.5.97, whose older bone-scaling interface SLIF NG binds separately[/*]
[*][b]XPMSSE[/b] - the standard skeleton nodes[/*]
[*][b]PapyrusUtil[/b] - used once, to read an old SLIF save during migration[/*]
[/list]



[size=4][b]📥 Installation[/b][/size]

[list]
[*][b]Uninstall or disable SexLab Inflation Framework[/b] and its patches. SLIF NG replaces it file for file[/*]
[*]Install with the FOMOD and answer its one question: which body did you build in BodySlide[/*]
[*]Load your game. There is no step 3[/*]
[*]If the save ran old SLIF, the import runs by itself and reports how many values it carried over[/*]
[*]Going back is a plain mod-manager swap: applied values live under old SLIF's own RaceMenu key, so the original framework finds and overwrites them[/*]
[/list]



[size=4][b]🔄 What Changed vs Old SLIF[/b][/size]
[spoiler]
[list]
[*][b]Engine[/b] - ~11,700 lines of Papyrus with per-step body rebuilds -> native C++, one coalesced apply per actor, unchanged values skipped[/*]
[*][b]Setup[/b] - nine MCM pages and hand-edited JSON (morphs shipped disabled) -> pick your body once in the installer[/*]
[*][b]The math[/b] - unchanged on purpose; the six calculation types were the field-tested part[/*]
[*][b]Bodies[/b] - one global config for the whole game -> per-actor profiles, UBE matched by race, verified slider names[/*]
[*][b]Several mods, one slider[/b] - morph contributions always stacked, even under "highest wins" -> the calculation type applies to sliders too[/*]
[*][b]Incremental inflation[/b] - a Papyrus drain loop, off by default -> a native ramp, on by default, with a speed slider[/*]
[*][b]Hidden nodes[/b] - morph side left stale while hidden -> sliders forced neutral, so a chastity belt flattens instead of dents[/*]
[*][b]Unknown bones[/b] - scaled whatever you named it, and a typo became an invisible bone scale -> rejected with a log line; growth goes to morphs or semantic regions instead[/*]
[*][b]Non-unique NPCs[/b] - transient scaling by default -> always persistent and self-healing[/*]
[*][b]State[/b] - hundreds of StorageUtil keys -> one compact co-save record[/*]
[*][b]Dropped[/b] - the grow/shrink/absorb spells, the actor scanner, the scrotum timer, the presets JSON API, 17 translations, and ~70 functions nothing calls[/*]
[*][b]New[/b] - automatic migration, magnitude knobs, the actor diagnostics page, semantic regions, batched writes, a read API, foreign-key detection[/*]
[/list]
[/spoiler]



[size=4][b]💻 For Mod Authors[/b][/size]

The [b]write API is old SLIF's, unchanged[/b] - [i]SLIF_Main.inflate[/i], [i]SLIF_Morph.morph[/i], the mod events, all pinned from 1.2.2 bytecode. If your mod worked against SLIF, it works here.

New on top:

[list]
[*][b]Query API[/b] in Papyrus ([i]IsTracked[/i], [i]GetTrackedActors[/i], [i]GetModsDriving[/i], [i]GetApplied[/i], [i]HasTarget[/i], [i]DrivenBy[/i], ...) and in C++ for other SKSE plugins[/*]
[*][b]SLIFNG_Settled[/b] mod event - act on a finished shape instead of polling[/*]
[*][b]Semantic regions[/b] ([i]region:weight[/i]) and batched writes[/*]
[*][b]Integration kit[/b] shipped as a separate API download[/*]
[*][url=https://crajjjj.github.io/SLIFNG/authors/overview/]Developer documentation[/url] - architecture, write API, query API, the aggregation math, the body profile format[/*]
[/list]



[size=4][b]❤️ Credits[/b][/size]

[list]
[*][b][i]qotsafan[/i][/b] - author of [b]SexLab Inflation Framework[/b], and everyone who contributed to it over the years. SLIF NG is a reimplementation of their framework: the API, the node vocabulary, the six calculation types and their formulas are all theirs, kept deliberately intact so the ecosystem built on SLIF keeps running. None of this exists without that work[/*]
[*][i]expired6978[/i] - RaceMenu / skee, which does the actual body changes[/*]
[*]The [b]SKSE[/b] team, and [b]CommonLibSSE-NG[/b] (Ryan-rsm-McKenzie, alandtse and contributors)[/*]
[*][i]Ousnius[/i] & [i]Caliente[/i] - BodySlide, and the body authors whose slider sets the profiles target[/*]
[*]The authors of the consumer mods this was tested against: Beeing Female, Fill Her Up, Sexlab Survival, Estrus Chaurus, Devious Devices, Devious Interests, Milk Mod Economy[/*]
[/list]

[i]Note:[/i] SLIF NG ships a clean-room plugin that reuses the [b]SexLab Inflation Framework.esp[/b] name and its FormIDs. That is not a copy of qotsafan's plugin - it is how existing mods detect the framework without needing a single patch.

---

[size=3][b]📜 Source Code[/b][/size]
[url=https://github.com/crajjjj/SLIFNG]GitHub Repository[/url] • [url=https://crajjjj.github.io/SLIFNG/]Documentation[/url]
