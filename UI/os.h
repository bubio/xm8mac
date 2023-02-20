//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ operating system ]
//

#ifdef SDL

#ifndef OS_H
#define OS_H

//
// Windows
//
#ifdef _WIN32
#ifdef _MSC_VER
#pragma warning(disable:4127)
#endif _MSC_VER
#include "SDL.h"
#include "SDL_syswm.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#endif // _WIN32

//
// Linux
//
#if defined(__linux__) && !defined(__ANDROID__)
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#endif // __linux__ && !__ANDROID__

//
// Android
//
#ifdef __ANDROID__
#include "SDL.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#endif // __ANDROID__

#endif // OS_H

#endif // SDL
