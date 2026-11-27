# Beyond Expansion — verification (OpenGame step 4)

## Automated checks (all pass)
`make test`, with address and undefined-behaviour sanitizers:
- **world_test (new)**: all 8 worlds are 3,600 m across; the boss clearing is 900–1,050 m from the start; the start,
  clearing, crystal, 6 Story Stones and 3 Signal Towers stand on solid ground; nothing sits in the clearing; props and food
  are filed in the right chunks; knocking a tower down switches its robots off; every sky island can be reached by bridge.
- dragon, boss, game, co-op, sky and tour tests: green. The tour beats Stormtalon in 44.7 s (was 40.5 s; the limit is 150 s).

| world | props | food | drones | hounds | lakes | build time (PC) |
|---|---|---|---|---|---|---|
| Everleaf Wilds | 108,892 | 32,042 | 96 | 70 | 10 | 0.3 s |
| Sky Kingdom | ~9,000 on 128 islands (land = 100x the old 8) | ~6,900 | 95 | 0 | 0 | 0.06 s |
| other six | 107–110k | 23–32k | 95–96 | 70–72 | 0–10 | 0.3–0.6 s |

## Draw calls per frame (stub renderer, Wii U settings)
| | before | after |
|---|---|---|
| Sky Kingdom (tour) | 177 | 245 |
| Everleaf Wilds | 838 | 926 |
| Frostpeak | 827 | 973 |
| emulator tour build | 174 | 130 |

## Look check
PC build of the phone code under a virtual screen, one view per world: `design/beyond_looks.jpg`
(forest vista, Sky Kingdom, Frostpeak, desert, Coral Sea, volcano, a Signal Tower, a Story Stone).

## Not checked here (known gaps)
1. Real Wii U frame rate and memory (the CI emulator build is the next check; ask for the log's FPS lines on the console).
2. World load time on the Wii U: estimated 3–6 s per world (0.3–0.6 s on a PC).
3. Lake-shore foam is stair-stepped on 3 m cells (same method as the old pond).
4. Story text is a draft: the designer should read and change it.
5. Fun over the bigger distances is untested with a real player (a dragonet walks about 2.5 minutes to the boss).
