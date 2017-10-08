#pragma once
#include "stdafx.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

struct AppInfo
{
	struct HWND__* wndHdl;
	struct HINSTANCE__* instance;
	int curW;
	int curH;
};

extern AppInfo GAppInfo;

struct HWND__* os_makeWindow(struct HINSTANCE__* Instance, const char* title, unsigned int width, unsigned int height);
void		os_setResizeCallback(void (*cb)(int, int));
void		os_handleEvents();

void		os_pollInput();
double		os_getMilliseconds();