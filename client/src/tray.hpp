#ifndef _TRAY_HPP__
#define _TRAY_HPP__


#ifdef _WIN32
#define TRAY_WINAPI 1
#define TRAY_ICON "icon0.ico"
#elif defined(__linux__)
#define TRAY_APPINDICATOR 1
#define TRAY_ICON "games"
#endif

#include "../deps/tray/tray.h"

void tray_thread_fn(void);

#endif // _TRAY_HPP__