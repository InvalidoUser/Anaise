# The New Gods: research and implementation notes

Researched 2026-10-23. This chapter uses an original plot, characters and art.
No panels, dialogue, character designs or music from the referenced comics or
adaptations are incorporated.

## Creative references

- [Lovecraft, The Call of Cthulhu, original text](https://www.gutenberg.org/cache/epub/68283/pg68283-images.html):
  immense scale, an octopoid head, wings and alien masonry informed the witness
  silhouette and arena architecture. The procedural art is drawn in code.
- [Tower of God, official WEBTOON synopsis](https://m.webtoons.com/en/fantasy/tower-of-god/list?title_no=95)
  and [official volume synopsis](https://shop.webtoon.com/products/tower-of-god-vol-1):
  ascent through tests, companions with private ambitions and gatekeepers
  informed the chapter structure. The oath, memory, endurance and contract
  trials here are original.
- [The Tutorial Is Too Tough!, official Tapas synopsis](https://tapas.io/series/the-tutorial-is-too-tough/info):
  a hostile tutorial world and survival through understanding its rules
  informed the encounter telegraphs, stamina economy and encounter retries.
  Defeat retries the current encounter rather than forcing another ritual.

## Combat research translated into mechanics

Theoi publishes classical source excerpts as well as editorial summaries. The
linked primary-text passages are the basis for the mythological attributes:

- [Ares: Homer, Iliad 5.592 and 18.516; Hesiod, Shield 191](https://www.theoi.com/Olympios/Ares.html):
  martial armour, an aggressive advance and a forceful close-range weapon.
- [Athena: Quintus Smyrnaeus, Fall of Troy 14.386](https://www.theoi.com/Olympios/Athena.html):
  armour, the aegis and disciplined force become a shield that deflects reckless
  swings and an opening after a telegraphed thrust.
- [Zeus: classical iconography and source collection](https://www.theoi.com/Olympios/Zeus.html):
  lightning becomes a delayed ground strike. Movement answers the sky attack;
  the sword still deals the winning damage at close range.

These mechanics are design interpretations, not claims that the myths describe
this game or this sequence of battles.

## Technical research and constraints

[Glenn Fiedler, Fix Your Timestep!](https://gafferongames.com/post/fix_your_timestep/)
informed bounded simulation substeps. The frame timer supplies elapsed time,
capped at 120 ms and subdivided into at most 10 ms updates. Focus loss pauses
the sequence. Rendering does not advance game state, and headless previews use
only the virtual clock.

The arena uses software perspective floor casting and tile DDA wall casting.
Armoured opponents, companions and objects project from world coordinates;
column visibility checks occlude them behind masonry. Art, lighting and mist
remain binary black and white. There is no added runtime dependency. This is
a retro software 3D scene with projected figures, not a polygon engine.

The tower is a complete four-floor offline opening chapter with three named
recruitable NPCs and two endings. Live multiplayer would require a separate
networking/server design; NPC climbers are never represented as connected users.

## Audio sources and rights

- [Bobjt, Original hymn](https://opengameart.org/content/original-hymn): the
  source page explicitly marks the instrumental recording CC0. Runtime
  processing adds a slow volume/pitch ascent, soft saturation and two echoes.
- [Kenney, RPG Audio](https://kenney.nl/assets/rpg-audio): CC0 in the official
  page and archive license; metallic click and blade swish.
- [Kenney, Impact Sounds](https://kenney.nl/assets/impact-sounds): CC0 in the
  official page and archive license; metal hit, parry and bell impact.
- [techiew, Ambient horror](https://opengameart.org/content/ambient-horror):
  explicitly CC0 on its source page; slowed horror swell for the witness reveal.
- [Creative Commons, CC0 deed](https://creativecommons.org/publicdomain/zero/1.0/):
  public-domain dedication permitting copying, modification and redistribution.

Exact filenames, edits and resources are recorded in
[`assets/audio/CREDITS.txt`](../assets/audio/CREDITS.txt). Reproduction scripts
are development tools only; the app embeds the converted PCM data and performs
no downloads. All voices use the existing mute ramp. The mixer supports eight
overlapping sound effects and bounds the final PCM sample output.
