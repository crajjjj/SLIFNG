SLIF NG - region overlays
=========================

Every .ini in THIS folder is an overlay: its sections are merged into the body
profile SLIF NG resolves for each actor, without editing that profile.

Why this exists
---------------
A body profile (..\*.ini) is a whole file, and only ONE of them wins per actor.
So a mod that wants to add a custom region - or a user who wants to correct a
slider name - could not do it without overwriting the profile the installer
wrote for their body. An overlay adds sections instead of replacing the file.

Two uses:

  1. A CONSUMER MOD ships the region it needs. Name the file after your mod so
     it can never collide with anything: "SGO4-3BA.ini", not "weight.ini".

  2. A USER fixes or tunes a region for their own body. Shipped profiles are
     overwritten when you update SLIF NG; an overlay of your own is not.

Format
------
Exactly the body-profile format, plus an [Overlay] section saying which body it
applies to. Profile= matches a profile's Name= (see the top of any profile in
the parent folder), or "*" for every body.

    [Overlay]
    Profile=CBBE 3BA

    [MuscleMass]
    FullScale=2.0
    Morph1=MuscleDefinition
    Morph1Max=0.7
    Morph2=ChubbyWaist
    Morph2Max=-0.2        ; negative runs the slider in reverse

A mod supporting several bodies ships one file per body - "MyMod-3BA.ini",
"MyMod-UBE.ini" - each naming its own Profile=. Sliders are body-specific, so
there is rarely one answer for all of them; "*" is for the uncommon case of a
slider spelled the same everywhere (the shipped Ovibody.ini is one: Ovibody
ships its egg sliders under one set of names on every body it supports).

A slider may fill over a WINDOW of the range instead of the whole of it, with
MorphNStart / MorphNEnd as fractions of FullScale: zero below Start, Max from
End on, a straight line between. It may also switch OFF again over a second
window, MorphNOffStart / MorphNOffEnd. Together they put a sequence on one
key - Ovibody.ini grows every egg over the first unit and then lets them
leave one at a time over the second:

    [Oviposition]
    FullScale=1.0
    Morph1=Egg_1
    Morph1Max=1.0
    Morph1Start=0.0
    Morph1End=1.0
    Morph1OffStart=1.0
    Morph1OffEnd=1.05
    Morph2=Egg_2
    Morph2Max=1.0
    Morph2Start=0.0
    Morph2End=1.0
    Morph2OffStart=1.05
    Morph2OffEnd=1.1

Without any window keys a slider keeps the plain line through neutral,
unclamped. A section takes up to 32 MorphN entries.

Consumers then address the section as "region:<name>", case-insensitively:

    SLIF_Main.inflate(akActor, "My Mod", "region:musclemass", 1.4)

Rules
-----
- A whole SECTION is the unit. An overlay section REPLACES the profile's, so
  you can correct a shipped slider name by redefining the section.
- Files apply in alphabetical order and the last one wins, so name yours
  "zz-*.ini" if you want it to beat everything else. A collision between two
  overlays is written to SLIFNG.log - it is never silent.
- Overlays can redefine canonical keys ([slif_belly], ...) too, not just custom
  regions. That is the supported way to fix a wrong slider for your body.
- A region has NO skeleton bone behind it: if no profile or overlay defines the
  section, inflating it is a logged no-op. Canonical keys always fall back to
  node scaling instead.
- Consumers should check SLIFNG.HasTarget(actor, "region:name") before sending,
  and fall back to a canonical key when it is false. Needs SLIFNG.GetVersion()
  >= 3.

Check SLIFNG.log after a load: every overlay logs what it loaded and which
profiles it merged into.
