// kk_files_web.c - the game's files in a web browser (the PC version built with Emscripten).
// The game opens files with fopen as usual (kk_files.h turns fopen into kk_fopen). The first time a file under content/ is
// opened it is downloaded from the website (showing progress on the page) and kept in memory; files the site does not have
// (listed in content/files.txt) fail at once without asking the server. Saves live in the browser's own storage (IndexedDB)
// and are written back a moment after the game saves.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <emscripten.h>
#undef fopen

static char *g_list; static int g_dirty;

/* download url into the in-memory file system at path; returns 1 if it worked */
EM_ASYNC_JS(int, web_fetch, (const char *url, const char *path), {
    const u = UTF8ToString(url), p = UTF8ToString(path);
    const show = (txt) => { if (typeof Module.dbProgress === 'function') Module.dbProgress(txt); };
    try {
        const r = await fetch(u);
        if (!r.ok) return 0;
        const total = +r.headers.get('Content-Length') || 0; const parts = []; let got = 0;
        const rd = r.body.getReader(); const name = u.split('/').pop();
        for (;;) { const { done, value } = await rd.read(); if (done) break; parts.push(value); got += value.length;
            if (total > 2000000) show('Loading ' + name + '  ' + Math.round(got / 1e6) + ' / ' + Math.round(total / 1e6) + ' MB'); }
        const data = new Uint8Array(got); let o = 0; for (const c of parts) { data.set(c, o); o += c.length; }
        const dir = p.substring(0, p.lastIndexOf('/')); let acc = '';
        for (const seg of dir.split('/')) { if (!seg) continue; acc += '/' + seg; try { FS.mkdir(acc); } catch (e) {} }
        FS.writeFile(p, data); show('');
        return 1;
    } catch (e) { console.log('fetch failed', u, e); return 0; }
});

/* the saves: IndexedDB mounted where SDL keeps its files; read once at start */
EM_ASYNC_JS(void, web_mount_saves, (void), {
    try { FS.mkdir('/libsdl'); } catch (e) {}
    FS.mount(IDBFS, {}, '/libsdl');
    await new Promise((ok) => FS.syncfs(true, (err) => { if (err) console.log('saves', err); ok(); }));
});
EM_JS(void, web_flush_saves, (void), { FS.syncfs(false, (err) => { if (err) console.log('save', err); }); });

void kk_files_init(void) {
    web_mount_saves();
    if (web_fetch("content/files.txt", "/web/files.txt")) {
        FILE *f = fopen("/web/files.txt", "rb"); if (f) { fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); g_list = malloc((size_t)n + 2); g_list[0] = '\n'; fread(g_list + 1, 1, (size_t)n, f); g_list[n + 1] = 0; fclose(f); } } }

static int listed(const char *rel) {   /* is "content/..." on the site? (no list: try anyway) */
    if (!g_list) return 1; char key[300]; snprintf(key, sizeof key, "\n%s\n", rel); return strstr(g_list, key) != NULL; }

FILE *kk_fopen(const char *path, const char *mode) {
    if (mode[0] != 'r') { g_dirty = 1; return fopen(path, mode); }   /* a save */
    FILE *f = fopen(path, mode); if (f) return f;
    if (strncmp(path, "content/", 8)) return NULL;
    if (!listed(path)) return NULL;
    char dst[320]; snprintf(dst, sizeof dst, "/%s", path);
    struct stat st; if (stat(dst, &st) != 0 && !web_fetch(path, dst)) return NULL;
    return fopen(dst, mode); }

/* called every frame: write the saves to the browser's storage a little after the game changed them */
void kk_web_frame(void) { static int wait; if (g_dirty) { g_dirty = 0; wait = 30; } if (wait > 0 && --wait == 0) web_flush_saves(); }
