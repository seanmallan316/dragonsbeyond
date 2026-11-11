// kk_files.c - open the game's files straight from inside the Android app (see kk_files.h).
#include <stdio.h>
#include <string.h>
#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <SDL.h>
#include <SDL_system.h>
#undef fopen   /* the real one, here */

static AAssetManager *g_am;
static jobject g_am_ref;

void kk_files_init(void) {
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv(); jobject act = (jobject)SDL_AndroidGetActivity(); if (!env || !act) return;
    jclass c = (*env)->GetObjectClass(env, act);
    jmethodID m = (*env)->GetMethodID(env, c, "getAssets", "()Landroid/content/res/AssetManager;");
    jobject am = (*env)->CallObjectMethod(env, act, m);
    g_am_ref = (*env)->NewGlobalRef(env, am); g_am = AAssetManager_fromJava(env, g_am_ref);
    (*env)->DeleteLocalRef(env, am); (*env)->DeleteLocalRef(env, c); (*env)->DeleteLocalRef(env, act);
}

static int a_read(void *c, char *b, int n) { return AAsset_read((AAsset *)c, b, (size_t)n); }
static fpos_t a_seek(void *c, fpos_t o, int w) { return (fpos_t)AAsset_seek64((AAsset *)c, (off64_t)o, w); }
static int a_close(void *c) { AAsset_close((AAsset *)c); return 0; }

FILE *kk_fopen(const char *path, const char *mode) {
    if (g_am && path && !strncmp(path, "content/", 8) && mode[0] == 'r' && !strchr(mode, '+')) {
        AAsset *a = AAssetManager_open(g_am, path, AASSET_MODE_RANDOM); if (!a) return NULL;
        FILE *f = funopen(a, a_read, NULL, a_seek, a_close); if (!f) AAsset_close(a);
        return f;
    }
    return fopen(path, mode);
}
