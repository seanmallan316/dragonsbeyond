/* Bits the Effekseer GL renderer expects that OpenGL ES 3 headers leave out (it is normally built with its own CMake setup). */
#ifndef EFK_PREFIX_H
#define EFK_PREFIX_H
#ifdef __cplusplus
#include <EGL/egl.h>
#include <stdio.h>
#ifdef __ANDROID__
extern "C" FILE *kk_fopen(const char *path, const char *mode);   /* effect files are read straight out of the app */
#define fopen kk_fopen
#endif
#endif
#define GL_BGRA 0x80E1
#define GL_DEPTH_COMPONENT32 0x81A7
#endif
