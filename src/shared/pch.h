#ifdef __GNUC__
#define gamma __gamma
#endif

#include <math.h>

#ifdef __GNUC__
#undef gamma
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <limits.h>
#include <assert.h>
#ifdef __GNUC__
#include <new>
#else
#include <new.h>
#endif
#include <time.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_opengl.h>

#include <enet/enet.h>

#ifdef WIN32
  #define _WINDOWS
  #ifndef __GNUC__
    #define ZLIB_DLL
  #endif
#endif
#include <zlib.h>

