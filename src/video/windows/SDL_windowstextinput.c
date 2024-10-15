/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
/*
  Screen keyboard and text input backend
  for GDK platforms.
*/
#include "SDL_internal.h"
#include "SDL_gdktextinput.h"

#ifdef SDL_WIN_TEXTINPUT

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../../events/SDL_keyboard_c.h"
#include "../windows/SDL_windowsvideo.h"

bool WIN_HasScreenKeyboardSupport(SDL_VideoDevice *_this)
{
    return true;
}

void WIN_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID props) {

}

void WIN_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window) 
{

}

bool WIN_IsScreenKeyboardShown(SDL_VideoDevice *_this, SDL_Window *window) 
{
    return false;
}


#ifdef __cplusplus
}
#endif

#endif