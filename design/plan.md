# Dragons Beyond — "Beyond Expansion" plan

Made with the OpenGame method (github.com/nexu-io/opengame, Apache-2.0): start from the playable game we already have,
keep the mechanics that work, expand the world and story, then verify with automated checks and screenshots
before publishing. Story additions are a DRAFT for the designer to approve or change.

## 1. First-glance identity
A small forest dragonet in a huge, living wild. Every world now stretches past the fog in every direction:
3.6 km x 3.6 km of hills, lakes, ridges and named places (it was 360 m x 360 m).

## 2. Native fun engine (unchanged, now with room to breathe)
Eat → grow → fly → rescue. Growth still comes from food; the bigger map means hunting further, finding the
best meadows and lakes, and flying long distances once grown.

## 3. Signature twist: Kaine's Signal Towers
Each world has 3 black Signal Towers spread across the map. They send Kaine's control signal to his robots.
Knock one down (tail whip, claw or a power) and the robots near it shut down for good, the dragon gets a
growth boost, and a story card plays. Knock all 3 down and that world's boss loses one hit of armour.
Towers are optional: a young player can still go straight to the boss.

## 4. Peak moment
Flying as a grown Beyonder from the start meadow to the far boss clearing (about 1 km away), seeing the
world's landmark rise out of the fog, a Signal Tower's red light blinking on a far ridge.

## 5. Expanded story (DRAFT for the designer)
- Prologue grows from 6 to 9 cards: who Everwing is, the seven dragon families, how Kaine got his name.
- 6 Story Stones per world (48 in all) tell the history piece by piece: Everwing, the seven families, the
  boy who wanted a dragon, the stolen black feather, the call that drew all seven parents "beyond the mountains",
  and finally where they went. Collecting them is shown on the map.
- An arrival card for every world, and each world's ending grows from 3 cards to 4.
- The ending now points "beyond": the stones reveal the parents were lured past the mountains by Kaine's
  feather whistle, so the seven dragons fly off to find them — the hook for a sequel.

## 6. World expansion (100x area per map)
| | before | after |
|---|---|---|
| map size | 360 m | 3,600 m (100x area) |
| height samples | 144 x 144 | 1,153 x 1,153 (3.1 m apart) |
| scenery props | 1,100 | up to 120,000 |
| food | 340 | up to 36,000 |
| robots (drones/hounds/thieves) | 8 / 6 / 4 | up to 96 / 72 / 32, in camps |
| lakes | 1 | up to 10 |
| sky islands | 8 | 80+, joined by bridges |
| named regions | 0 | 6 per world |

Why area and not width: 100x the width (36 km) would take a dragonet 90 minutes to cross on foot, with nothing
to find between. 100x the area keeps the same density of food, trees and robots everywhere.

Engine changes this needs:
- the ground is streamed in 100 m chunks: full detail near the dragon, a coarse version out to the fog;
- trees, food and robots are found through a per-chunk index instead of checking every one;
- grass, flowers, clouds, the sea surface, the cave roof and far mountains follow the camera;
- the world's own big shapes (mountain ranges, valleys, plateaus) are added on top of the small hills so
  every region looks different.

## 7. Invariants (hard fails — the build is not done if any of these break)
1. `make test` stays green (dragon, boss, game, co-op, sky and tour tests).
2. Every world is 3,600 m across; the boss clearing is at least 700 m from the start meadow.
3. The start meadow, every Story Stone, every Signal Tower, the egg crystal and the boss clearing stand on solid,
   walkable ground (not in a lake, not in the cloud gaps, not under the sea except in the Coral Sea).
4. No food or prop sits inside the boss clearing; the clearing's ring trees are still there.
5. Ground meshes stay inside a memory budget (live chunks are capped) and building chunks never stalls a frame
   for more than a set number of chunk builds.
6. The scripted tour still beats Stormtalon within 150 game seconds.

## 8. Verification plan
- New `tests/world_test.c`: checks invariants 2–5 for all 8 worlds.
- PC build of the phone code under a virtual screen: screenshots of the start meadow, a far view, a Signal
  Tower and a Story Stone, put together on one contact sheet and checked by eye.
- Results written to `design/verification.md` with known gaps.
