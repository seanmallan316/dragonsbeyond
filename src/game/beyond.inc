/* beyond.inc - the Beyond Expansion's story and places (included by game.c):
   named regions in every world, six Story Stones per world that tell the history piece by piece, and Kaine's three Signal
   Towers per world (knock one down and the robots it controls switch off for good; all three and the boss loses armour).
   The story words are a draft for the designer to change. */

static const char *REGION_NAMES[WK_N][W_NREGIONS] = {
  { "SUNRISE MEADOW", "IRONHORN'S CLEARING", "MOONFERN HOLLOW", "THE OLD ROOTS", "BRAMBLEBACK RIDGE", "WHISPERING LAKES" },
  { "HOME ISLAND", "STORMTALON'S PERCH", "THE HIGH BRIDGES", "CLOUDCOMB ISLES", "WINDWHISTLE REACH", "THE FLOATING GARDENS" },
  { "FIRST SNOW VALLEY", "FROSTFANG'S PASS", "THE WINTER PALACE RUINS", "AURORA HEIGHTS", "ICICLE WOODS", "THE FROZEN LAKES" },
  { "THE GLOWING GATE", "SHARDCLAW'S HALL", "THE HUMMING DEEPS", "AMETHYST FALLS", "THE LANTERN MAZE", "STARLIGHT GROTTO" },
  { "OASIS SPRINGS", "DUNEBREAKER'S PIT", "THE SINGING DUNES", "SANDSTONE SPIRES", "MIRAGE FLATS", "CACTUS CANYON" },
  { "SANDY HOME ISLE", "TIDEHORN'S REEF", "TURTLE BAY", "THE KELP FOREST", "THE WHALE DEEPS", "THE CORAL KINGDOM" },
  { "ASHFALL RIM", "MAGMAHORN'S FORGE", "THE CINDER WOODS", "THE LAVA LAKES", "SMOKESTACK RIDGE", "EVERWING'S CRADLE" },
  { "THE BROKEN GATE", "THE MECH DRAGON'S ARENA", "THE CAGE YARDS", "THUNDER CRAGS", "THE STOLEN NESTS", "KAINE'S WORKSHOP" } };

/* the Story Stones: read in any order, but the last one in every world is always a parent dragon's message */
static const char *LORE[WK_N][W_NSTONES] = {
  { "THE STONE IS COVERED IN MOSS. IT READS: HERE THE FOREST DRAGONS EAT AND GROW. BERRIES, MUSHROOMS, GRUBS AND FISH: THE MORE YOU EAT, THE BIGGER YOU GET.",
    "THE FOREST DRAGONS PLANTED EVERY TREE IN EVERLEAF WILDS, ONE SEED FOR EVERY EGG THAT EVER HATCHED HERE. THE OLDEST OAK IS OLDER THAN THE MOUNTAINS.",
    "EVERWING, THE MASTER DRAGON, HALF NIGHT AND HALF SNOW, BREATHED A SPARK OF ITS OWN LAND INTO EVERY NEW EGG. THAT SPARK BECOMES A DRAGON'S POWER.",
    "A BOY ONCE CAME TO THE WILDS, BEGGING FOR A DRAGON OF HIS OWN. EVERWING SAID NO: A DRAGON BELONGS TO NO ONE. THE BOY NEVER FORGOT IT.",
    "THE BOY'S NAME WAS KAINE. ONE NIGHT HE CREPT UP ON THE SLEEPING EVERWING AND PLUCKED A SINGLE BLACK FEATHER. THAT IS HOW HE GOT HIS NAME: BLACKFEATHER.",
    "SCRATCHED UNDER THE MOSS IN A PARENT DRAGON'S CLAW: WE HEARD A WHISTLE BEYOND THE MOUNTAINS. WE WILL BE BACK BY MORNING. WAIT FOR US, LITTLE ONE." },
  { "THE SKY DRAGONS BUILT THESE STONE BRIDGES SO THEIR DRAGONETS COULD WALK FROM ISLAND TO ISLAND BEFORE THEIR WINGS WERE STRONG ENOUGH TO FLY.",
    "NEVER FALL ASLEEP NEAR THE EDGE, SAYS THE STONE. THE CLOUD SEA IS SOFT, BUT IT GOES DOWN FOREVER.",
    "THE GOLDEN RINGS ARE THE SKY DRAGONS' OLD RACE TRACK. FLY THROUGH ONE AND THE WIND WILL GIVE YOU A PUSH.",
    "KAINE'S FEATHER DRONES WERE MADE WITH THE BLACK FEATHER'S DARK MAGIC. THEY OBEY HIS SIGNAL TOWERS. KNOCK A TOWER DOWN AND ITS ROBOTS FALL ASLEEP.",
    "STORMTALON WAS KAINE'S FIRST GIANT ROBOT. HE GAVE IT CLAWS LIKE A DRAGON'S, BECAUSE HE NEVER GOT A DRAGON OF HIS OWN.",
    "A PARENT'S CLAW MARKS: THE WHISTLE CAME FROM THE NORTH, PAST THE SNOWY PEAKS. EVERY GROWN-UP DRAGON IN THE WORLD HEARD IT." },
  { "THE WINTER PALACE ONCE STOOD HERE, CARVED FROM ICE BY THE SNOW DRAGONS. ONLY THE PINES REMEMBER WHERE IT WAS.",
    "SNOW DRAGONS SLEEP UNDER THE SNOW ALL WINTER AND WAKE UP HUNGRY. THEY CAN EAT THEIR OWN WEIGHT IN FISH IN ONE DAY.",
    "THE GREEN LIGHTS IN THE NIGHT SKY ARE EVERWING'S BREATH, SAY THE OLD STORIES. WHEN THEY DANCE, THE MASTER DRAGON IS DREAMING.",
    "KAINE CAME HERE WITH A WAGON OF CAGES AND A SACK OF MEAT. HE TOOK THE SNOW DRAGONET WHILE THE WIND HOWLED.",
    "FROSTFANG HAS ANTLERS OF ICE. WHEN IT CHARGES, IT CANNOT STOP. LET IT RUN INTO A PINE TREE, THEN STRIKE.",
    "PARENT CLAW MARKS, DEEP IN THE ICE: THE WHISTLE IS A TRICK. IT IS KAINE'S, MADE FROM THE BLACK FEATHER. WE ARE TRAPPED BEYOND THE MOUNTAINS." },
  { "THE CRYSTAL DRAGONS GREW THESE CRYSTALS THEMSELVES, ONE SONG AT A TIME. TAP ONE AND YOU CAN STILL HEAR IT HUM.",
    "THIS CAVERN GOES ON AND ON IN THE DARK. THE CRYSTALS ARE YOUR LANTERNS: FOLLOW THEIR GLOW.",
    "THE EGG CRYSTAL HIDDEN IN EVERY LAND HOLDS A LITTLE OF THE SPARK EVERWING GAVE THE EGGS. FIND IT AND YOU GROW STRONGER.",
    "KAINE TRIED TO STEAL THE CRYSTALS TO POWER HIS MACHINES. THEY WENT DARK IN HIS HANDS. CRYSTALS ONLY SHINE FOR DRAGONS.",
    "SHARDCLAW IS MADE OF STOLEN CRYSTAL. IT HATES THE DARK, AND IT HATES DRAGONS EVEN MORE.",
    "PARENT CLAW MARKS: KAINE KEEPS THE WHISTLE IN HIS TOWER. BREAK ITS SPELL AND WE CAN COME HOME." },
  { "THE SAND DRAGONS SWIM THROUGH THE DUNES LIKE FISH THROUGH WATER. THEIR EGGS ARE KEPT WARM IN THE HOT SAND.",
    "THE DUST DEVILS ARE PLAYFUL. RIDE ONE AND IT WILL THROW YOU HIGH INTO THE SKY.",
    "EVERY OASIS IN THE DESERT WAS DUG BY A SAND DRAGON'S TAIL, LONG AGO, SO NO TRAVELLER WOULD EVER BE THIRSTY.",
    "KAINE WAS NOT ALWAYS CRUEL. AS A BOY HE FED STRAY CATS. BUT HE WANTED A DRAGON SO MUCH THAT THE WANTING TURNED SOUR.",
    "DUNEBREAKER WAS BUILT TO DIG UP DRAGON EGGS. ITS HORNS ARE SHOVELS. DODGE, AND LET IT STICK INTO A CACTUS.",
    "PARENT CLAW MARKS IN THE SANDSTONE: WE CANNOT FLY HOME WHILE THE WHISTLE PLAYS. SAVE EACH OTHER, LITTLE ONES. THEN COME AND FIND US." },
  { "THE WATER DRAGONS LIVE BOTH ABOVE AND BELOW THE WAVES. THEY CAN HOLD THEIR BREATH FOR A WHOLE DAY.",
    "THE OLD WHALE HAS SWUM THESE SEAS SINCE BEFORE THE FIRST EGG HATCHED. HE IS EVERWING'S OLDEST FRIEND.",
    "EVERY CORAL TOWER IS A WATER DRAGON'S OLD NEST. THE SEA TURTLES LOOK AFTER THEM NOW.",
    "KAINE CANNOT SWIM. THAT IS WHY HE BUILT TIDEHORN, A ROBOT THAT WALKS ON THE SEA BED.",
    "TIDEHORN CHARGES THROUGH THE WATER LIKE A SHARK. LET IT HIT THE CORAL, THEN SWIM IN AND STRIKE.",
    "PARENT CLAW MARKS ON A SHELL: THE WHISTLE CALLS LOUDER NOW. KAINE WANTS EVERY DRAGON. DO NOT LET HIM KEEP EVERWING." },
  { "THE LAVA DRAGONS BATHE IN FIRE THE WAY OTHER DRAGONS BATHE IN WATER. DO NOT TRY IT UNLESS YOU ARE ONE.",
    "WATCH FOR GLOWING CRACKS IN THE GROUND: A FIRE GEYSER IS ABOUT TO BLOW.",
    "THIS VOLCANO IS WHERE EVERWING WAS BORN, FROM AN EGG AS BIG AS A HOUSE, HALF NIGHT AND HALF SNOW.",
    "KAINE BUILT HIS MECH DRAGON IN THIS VOLCANO. HE WORKED FOR SEVEN YEARS, ONE YEAR FOR EVERY DRAGON HE WANTED TO OWN.",
    "MAGMAHORN GUARDS THE LAST CAGE. ITS HORNS GLOW RED HOT, BUT IT IS SLOW TO TURN.",
    "PARENT CLAW MARKS, STILL WARM: WHEN EVERWING IS FREE, ITS ROAR WILL BREAK THE WHISTLE'S SPELL. THEN WE CAN COME HOME, OR YOU CAN COME TO US." },
  { "BLACKFEATHER KEEP WAS BUILT FROM THE STONES OF A HUNDRED STOLEN NESTS.",
    "KAINE'S MECH DRAGON SHOOTS RINGS OF FIRE. JUMP OR FLY OVER THEM, AND STRIKE WHEN IT IS STUCK.",
    "EVERWING IS CHAINED IN THE TOWER UNDER A DARK SPELL, MADE FROM ITS OWN STOLEN FEATHER.",
    "WRITTEN IN KAINE'S OWN HAND: ALL I EVER WANTED WAS ONE DRAGON TO BE MY FRIEND. NOW I HAVE SEVEN IN CAGES, AND NOT ONE OF THEM WILL LOOK AT ME.",
    "THE SIGNAL TOWERS HERE ARE THE STRONGEST OF ALL. EVERY ONE YOU BREAK MAKES THE MECH DRAGON WEAKER.",
    "A MAP SCRATCHED ON THE WALL: PAST THE MOUNTAINS, BEYOND THE EDGE OF EVERY MAP, A VALLEY WHERE SEVEN DRAGONS SLEEP. KAINE CALLED IT THE BEYOND." } };

/* said once when you first arrive in each world (a banner, the game does not stop) */
static const char *ARRIVE[WK_N] = {
  "EVERLEAF WILDS GOES ON FAR PAST THE FOG. FIND THE STORY STONES, AND KNOCK DOWN KAINE'S SIGNAL TOWERS.",
  "THE SKY KINGDOM: DOZENS OF ISLANDS FLOAT OVER THE CLOUDS, JOINED BY OLD STONE BRIDGES. STORMTALON'S ISLAND IS FAR AWAY.",
  "THE FROSTPEAK MOUNTAINS: REAL PEAKS NOW, WITH FROZEN LAKES IN THE VALLEYS. FROSTFANG WAITS BEYOND THE PASS.",
  "THE CRYSTAL CAVERNS GO ON AND ON IN THE DARK. FOLLOW THE GLOW OF THE CRYSTALS.",
  "THE SUNSCORCH DESERT: DUNE AFTER DUNE, AND AN OASIS WHERE YOU LEAST EXPECT ONE.",
  "THE CORAL SEA: ISLANDS FAR OUT ON THE WAVES, AND A WHOLE KINGDOM UNDER THEM.",
  "THE EMBER VOLCANO: RIVERS OF FIRE AND BLACK ASH AS FAR AS YOU CAN SEE. WATCH FOR GLOWING CRACKS.",
  "BLACKFEATHER KEEP: KAINE'S LAND. HIS STRONGEST SIGNAL TOWERS STAND HERE." };

static int g_region_shown = -1, g_region_cand = -1; static float g_region_t, g_region_banner_t;
static float g_tower_cd, g_tower_flash[W_NTOWERS];
static int g_tale_mode, g_tale_k; static char g_tale_buf[400];   /* the storyteller: 0 a rescue tale (TALE), 1 a Story Stone, 2 a fallen Signal Tower */
static float g_horizon_y;   /* the far mountains sit on the ground round you */

static int tale_count(void) { return g_tale_mode == 0 ? TALE_N : 1; }
static const char *tale_line(void) { return g_tale_mode == 0 ? TALE[g_tale_w][g_tale_i] : g_tale_mode == 1 ? LORE[g_tale_w][g_tale_k] : g_tale_buf; }
static void tale_start(int mode, int k) { g_tale_mode = mode; g_tale_k = k; g_tale_i = 0; g_tale_t = 0; g_tale_w = g_wk; }
static int popcount8(int m) { int n = 0; for (int i = 0; i < 8; i++) n += (m >> i) & 1; return n; }

/* a world was just made: put back which stones were read and which towers fell (and switch those towers' robots off) */
static void beyond_enter(void) {
    if (g_side) return;
    for (int i = 0; i < W_NSTONES; i++) g_world.stone[i].found = (g_wstones[g_wk] >> i) & 1;
    for (int i = 0; i < W_NTOWERS; i++) if ((g_wtowers[g_wk] >> i) & 1) { Attack a = {g_world.tower[i].x, g_world.tower[i].y + 2, g_world.tower[i].z, 1}; g_world.tower_hp[i] = 0.01f; world_hit_tower(&g_world, a, 1); }
    if (!g_pm[PM_TOWER].ok) propmodel_load(&g_pm[PM_TOWER], PM_NAMES[PM_TOWER]);
    { float s = 0; for (int k = 0; k < 12; k++) { float a = k * 0.5236f; s += world_height(&g_world, clampf(cosf(a) * 600, -W_SIZE / 2, W_SIZE / 2), clampf(sinf(a) * 600, -W_SIZE / 2, W_SIZE / 2)); } g_horizon_y = WK_ISLES(g_wk) ? 0 : fminf(30, fmaxf(-10, s / 12)); }
    g_region_shown = -1; g_region_cand = -1; g_region_banner_t = 0; }

/* the towers and stones, drawn with the scenery */
static void draw_landmarks(Vec3 eye) {
    if (g_side) return; gfx_state(ST_SOLID);
    for (int i = 0; i < W_NTOWERS; i++) { const Spot *T = &g_world.tower[i]; float dx = T->x - eye.x, dz = T->z - eye.z, d2 = dx * dx + dz * dz; if (d2 > 700 * 700) continue;
        if (g_pm[PM_TOWER].ok) { float yaw = i * 1.9f; Mat4 m = m4_mul(m4_translate(T->x, T->y - 1.2f, T->z), m4_rotate_y(yaw));
            if (T->found) m = m4_mul(m4_mul(m4_translate(T->x, T->y - 0.6f, T->z), m4_rotate_y(yaw)), m4_rotate_x(1.42f));   /* toppled */
            float fl = g_tower_flash[i] > 0 ? 1 + 2 * g_tower_flash[i] : 1, t = T->found ? 0.55f : fl; gfx_bind(g_pm[PM_TOWER].tex); gfx_draw(g_pm[PM_TOWER].mesh[d2 > 80 * 80], &m, t, T->found ? t : t * 0.9f, t, 1); } }
    if (g_pm[PM_ROCK].ok) for (int i = 0; i < W_NSTONES; i++) { const Spot *S = &g_world.stone[i]; float dx = S->x - eye.x, dz = S->z - eye.z, d2 = dx * dx + dz * dz; if (d2 > 260 * 260) continue;   /* a tall standing stone */
        Mat4 m = m4_mul(m4_mul(m4_translate(S->x, S->y - 0.4f, S->z), m4_rotate_y(i * 2.3f)), m4_scale(1.25f, 2.8f, 1.0f)); float t = S->found ? 1.0f : 1.35f;   /* pale, tall and flat-faced: it stands out from the boulders */
        gfx_bind(g_pm[PM_ROCK].tex); gfx_draw(g_pm[PM_ROCK].mesh[d2 > 50 * 50], &m, t * 1.0f, t * 1.04f, t * 1.15f, 1);
        for (int k = 0; k < 5; k++) { float a = i * 2.3f + k * 1.2566f; Mat4 r = m4_mul(m4_mul(m4_translate(S->x + cosf(a) * 2.6f, S->y - 0.2f, S->z + sinf(a) * 2.6f), m4_rotate_y(a)), m4_scale(0.35f, 0.3f, 0.35f));   /* a ring of small stones round it */
            gfx_draw(g_pm[PM_ROCK].mesh[1], &r, t * 0.9f, t * 0.92f, t, 1); } } }
/* their glows, into the sparkle batch: a tower's blinking red eye (seen from far off), a stone's golden rune until it is read */
static void landmark_glows(Vec3 eye) {
    if (g_side) return; Vec3 fw = v3_norm(v3(g_at.x - eye.x, 0, g_at.z - eye.z)), rt = v3(fw.z, 0, -fw.x);
    for (int i = 0; i < W_NTOWERS; i++) { const Spot *T = &g_world.tower[i]; if (T->found) continue; float dx = T->x - eye.x, dz = T->z - eye.z, d = sqrtf(dx * dx + dz * dz); if (d > 900) continue;
        float blink = fmodf(g_time * 0.9f + i * 0.33f, 1.0f) < 0.5f ? 1 : 0.25f, sz = 0.9f + d * 0.012f;   /* grows with distance so it can be seen across the valley */
        batch_quad(&g_b, v3(T->x, T->y + 13.5f, T->z), v3(rt.x * sz, 0, rt.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, 2.2f, 0.25f, 0.15f, blink); }
    for (int i = 0; i < W_NSTONES; i++) { const Spot *S = &g_world.stone[i]; if (S->found) continue; float dx = S->x - eye.x, dz = S->z - eye.z, d = sqrtf(dx * dx + dz * dz); if (d > 320) continue;
        float pu = 0.7f + 0.3f * sinf(g_time * 2.2f + i), sz = 1.5f + d * 0.008f;
        batch_quad(&g_b, v3(S->x, S->y + 5.2f + 0.25f * sinf(g_time * 1.3f + i), S->z), v3(rt.x * sz, 0, rt.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, 1.6f, 1.25f, 0.45f, pu); } }

/* every play frame: read a stone you walk up to, hits on towers, the region banner */
static void beyond_frame(Dragon *D, float dt) {
    if (g_side || g_tale_i >= 0) return;
    for (int i = 0; i < W_NTOWERS; i++) if (g_tower_flash[i] > 0) g_tower_flash[i] -= dt * 3;
    /* Story Stones: walk (or fly low) right up to one */
    for (int i = 0; i < W_NSTONES; i++) { Spot *S = &g_world.stone[i]; if (S->found) continue; float dx = S->x - D->x, dz = S->z - D->z;
        if (dx * dx + dz * dz < 4.0f * 4.0f && D->y < S->y + 6) { S->found = 1; g_wstones[g_wk] |= 1 << i; D->growth += 15; D->hearts = D->max_hearts; tale_start(1, i);
            sfx_play(SFX_COIN, 0.8f, 0); burst(v3(S->x, S->y + 3.6f, S->z), 30, 5, 0.25f, 1, 0.85f, 0.4f, 0.8f); save_progress(); return; } }
    /* Signal Towers: tail, claw, pounce or talons hit them; so does a power (a big area round the dragon) */
    if (g_tower_cd > 0) g_tower_cd -= dt;
    Attack a = {D->atk_x, D->atk_y, D->atk_z, D->atk_r}; float dmg = 0.34f;
    if (D == &g_dragon && g_power_t > 0.3f) { a.x = D->x; a.y = D->y; a.z = D->z; a.r = fmaxf(a.r, 9 + 2 * dragon_length(D->stage)); dmg = 0.5f; }
    if (a.r > 0 && g_tower_cd <= 0) { int r = world_hit_tower(&g_world, a, dmg);
        if (r <= -2) { int i = -2 - r; g_tower_cd = 0.45f; g_tower_flash[i] = 1; hit_juice(v3(g_world.tower[i].x, g_world.tower[i].y + 4, g_world.tower[i].z), 0.06f, 0.2f, 1, 0.3f, 0.2f); sfx_play(SFX_BLOCK, 0.8f, 0); platform_rumble(0.5f, 0.2f);
            int left = (int)ceilf(g_world.tower_hp[i] / dmg - 0.01f); snprintf(g_msg, sizeof g_msg, "THE SIGNAL TOWER CRACKS! %d MORE HIT%s", left, left == 1 ? "" : "S"); g_msg_t = 2.5f; }
        else if (r >= 0) { g_tower_cd = 0.6f; g_wtowers[g_wk] |= 1 << r; int down = world_towers_down(&g_world);
            hit_juice(v3(g_world.tower[r].x, g_world.tower[r].y + 6, g_world.tower[r].z), 0.12f, 0.5f, 1, 0.5f, 0.2f); burst(v3(g_world.tower[r].x, g_world.tower[r].y + 8, g_world.tower[r].z), 60, 9, 0.35f, 1, 0.4f, 0.2f, 1.0f);
            sfx_play(SFX_CRUMBLE, 1.0f, 0); sfx_play(SFX_WIN, 0.7f, 0); platform_rumble(1, 0.6f); D->growth += 60; D->hearts = D->max_hearts;
            if (down >= W_NTOWERS) { if (!g_rescued && g_boss.state != B_DOWN && g_boss.hp > 2) g_boss.hp--;
                snprintf(g_tale_buf, sizeof g_tale_buf, "THE LAST SIGNAL TOWER IN %s CRASHES DOWN! FAR AWAY, KAINE BLACKFEATHER'S WHISTLE SQUEAKS AND SPUTTERS. %s HAS LOST SOME OF ITS ARMOUR: ONE HIT FEWER TO WIN.", TH->name, boss_name()); }
            else snprintf(g_tale_buf, sizeof g_tale_buf, "THE SIGNAL TOWER CRASHES DOWN, AND KAINE'S ROBOTS AROUND IT GO QUIET FOR GOOD. %d MORE STILL STAND IN %s. KNOCK THEM ALL DOWN AND %s GETS WEAKER.", W_NTOWERS - down, TH->name, boss_name());
            tale_start(2, r); save_progress(); } }
    /* regions: a banner with the place's name when you have been in a new one for a moment */
    int rg = world_region_at(&g_world, D->x, D->z);
    if (rg != g_region_cand) { g_region_cand = rg; g_region_t = 0; } else g_region_t += dt;
    if (g_region_cand != g_region_shown && g_region_t > 1.2f) { g_region_banner_t = g_region_shown < 0 && g_title_t > 0 ? 0 : 4.0f; g_region_shown = g_region_cand; }   /* arriving: the world's own title card says where you are */
    if (g_region_banner_t > 0) g_region_banner_t -= dt;
    if (!((g_warrived >> g_wk) & 1) && g_state_t > 1.5f) { g_warrived |= 1 << g_wk; if (g_wk) { snprintf(g_msg, sizeof g_msg, "%s", ARRIVE[g_wk]); g_msg_t = 8; } } }

static void text_cg(const char *s, float y, float size, float a);
static void text_c(const char *s, float y, float size, float r, float g, float b, float a);
static void beyond_hud(void) {   /* the region's name, big and gold, for a few seconds */
    if (g_side || g_region_banner_t <= 0 || g_region_shown < 0 || g_title_t > 0) return;   /* (not over the world's title card) */
    float a = clampf(g_region_banner_t, 0, 1) * clampf((4.0f - g_region_banner_t) * 2, 0, 1);
    text_cg(REGION_NAMES[g_wk][g_region_shown], 196, 40, a);
    char sub[96]; snprintf(sub, sizeof sub, "%s   STORY STONES %d / %d   SIGNAL TOWERS DOWN %d / %d", TH->name, popcount8(g_wstones[g_wk]), W_NSTONES, popcount8(g_wtowers[g_wk]), W_NTOWERS);
    text_c(sub, 244, 18, 1, 0.95f, 0.8f, a * 0.9f); }
