#pragma once
#include "general_hooks.h"
#include "debug_printf.h"

using SDL_Window = void;
using SDL_Renderer = void;
using SDL_Texture = void;
using SDL_Surface = void;
using SDL_Cursor = void;
using SDL_GLContext = void*;
using SDL_RWops = void;

using Uint8 = uint8_t;
using Uint32 = uint32_t;
using Uint64 = uint64_t;
using Sint32 = int32_t;
using GLint = int;
using GLsizei = int;
using GLenum = unsigned int;

enum SDL_bool_enum { SDL_FALSE = 0, SDL_TRUE = 1 };
using SDL_bool = SDL_bool_enum;

struct SDL_Rect {
  int x, y;
  int w, h;
};

struct SDL_DisplayMode {
  Uint32 format;
  int w, h;
  int refresh_rate;
  void* driverdata;
};

struct SDL_Event {
  Uint32 type;
  Uint8 padding[56];
};

enum SDL_GLattr {
  SDL_GL_RED_SIZE,
  SDL_GL_GREEN_SIZE,
  SDL_GL_BLUE_SIZE,
  SDL_GL_ALPHA_SIZE,
  SDL_GL_BUFFER_SIZE,
  SDL_GL_DOUBLEBUFFER,
  SDL_GL_DEPTH_SIZE,
  SDL_GL_STENCIL_SIZE,
  SDL_GL_ACCUM_RED_SIZE,
  SDL_GL_ACCUM_GREEN_SIZE,
  SDL_GL_ACCUM_BLUE_SIZE,
  SDL_GL_ACCUM_ALPHA_SIZE,
  SDL_GL_STEREO,
  SDL_GL_MULTISAMPLEBUFFERS,
  SDL_GL_MULTISAMPLESAMPLES,
  SDL_GL_ACCELERATED_VISUAL,
  SDL_GL_CONTEXT_MAJOR_VERSION,
  SDL_GL_CONTEXT_MINOR_VERSION,
  SDL_GL_CONTEXT_FLAGS,
  SDL_GL_CONTEXT_PROFILE_MASK,
  SDL_GL_SHARE_WITH_CURRENT_CONTEXT,
  SDL_GL_FRAMEBUFFER_SRGB_CAPABLE
};

enum SDL_BlendMode {
  SDL_BLENDMODE_NONE = 0x00000000,
  SDL_BLENDMODE_BLEND = 0x00000001,
  SDL_BLENDMODE_ADD = 0x00000002,
  SDL_BLENDMODE_MOD = 0x00000004
};

enum SDL_SystemCursor {
  SDL_SYSTEM_CURSOR_ARROW,
  SDL_SYSTEM_CURSOR_IBEAM,
  SDL_SYSTEM_CURSOR_WAIT,
  SDL_SYSTEM_CURSOR_CROSSHAIR,
  SDL_SYSTEM_CURSOR_WAITARROW,
  SDL_SYSTEM_CURSOR_SIZENWSE,
  SDL_SYSTEM_CURSOR_SIZENESW,
  SDL_SYSTEM_CURSOR_SIZEWE,
  SDL_SYSTEM_CURSOR_SIZENS,
  SDL_SYSTEM_CURSOR_SIZEALL,
  SDL_SYSTEM_CURSOR_NO,
  SDL_SYSTEM_CURSOR_HAND,
  SDL_NUM_SYSTEM_CURSORS
};

using SDL_Keycode = Sint32;
const SDL_Keycode SDLK_UNKNOWN = 0;

enum SDL_Keymod {
  KMOD_NONE = 0x0000,
  KMOD_LSHIFT = 0x0001,
  KMOD_RSHIFT = 0x0002,
  KMOD_LCTRL = 0x0040,
  KMOD_RCTRL = 0x0080,
  KMOD_LALT = 0x0100,
  KMOD_RALT = 0x0200,
  KMOD_LGUI = 0x0400,
  KMOD_RGUI = 0x0800,
  KMOD_NUM = 0x1000,
  KMOD_CAPS = 0x2000,
  KMOD_MODE = 0x4000
};


using SDL_Init_fptr = int (*)(Uint32);
using SDL_Quit_fptr = void (*)(void);
using SDL_CreateWindow_fptr = SDL_Window* (*)(const char*, int, int, int, int, Uint32);
using SDL_DestroyWindow_fptr = void (*)(SDL_Window*);
using SDL_CreateRenderer_fptr = SDL_Renderer* (*)(SDL_Window*, int, Uint32);
using SDL_DestroyRenderer_fptr = void (*)(SDL_Renderer*);
using SDL_CreateRGBSurface_fptr = SDL_Surface* (*)(Uint32, int, int, int, Uint32, Uint32, Uint32, Uint32);
using SDL_FreeSurface_fptr = void (*)(SDL_Surface*);
using SDL_CreateTexture_fptr = SDL_Texture* (*)(SDL_Renderer*, Uint32, int, int, int);
using SDL_CreateTextureFromSurface_fptr = SDL_Texture* (*)(SDL_Renderer*, SDL_Surface*);
using SDL_DestroyTexture_fptr = void (*)(SDL_Texture*);
using SDL_RenderClear_fptr = int (*)(SDL_Renderer*);
using SDL_RenderCopy_fptr = int (*)(SDL_Renderer*, SDL_Texture*, const SDL_Rect*, const SDL_Rect*);
using SDL_RenderPresent_fptr = void (*)(SDL_Renderer*);
using SDL_SetRenderDrawColor_fptr = int (*)(SDL_Renderer*, Uint8, Uint8, Uint8, Uint8);
using SDL_RenderDrawPoint_fptr = int (*)(SDL_Renderer*, int, int);
using SDL_RenderDrawLine_fptr = int (*)(SDL_Renderer*, int, int, int, int);
using SDL_RenderDrawRect_fptr = int (*)(SDL_Renderer*, const SDL_Rect*);
using SDL_RenderFillRect_fptr = int (*)(SDL_Renderer*, const SDL_Rect*);
using SDL_PollEvent_fptr = int (*)(SDL_Event*);
using SDL_WaitEvent_fptr = int (*)(SDL_Event*);
using SDL_PushEvent_fptr = int (*)(SDL_Event*);
using SDL_GetTicks_fptr = Uint32 (*)(void);
using SDL_GetPerformanceCounter_fptr = Uint64 (*)(void);
using SDL_GetPerformanceFrequency_fptr = Uint64 (*)(void);
using SDL_Delay_fptr = void (*)(Uint32);
using SDL_LoadBMP_RW_fptr = SDL_Surface* (*)(SDL_RWops*, int);
using SDL_SaveBMP_RW_fptr = int (*)(SDL_Surface*, SDL_RWops*, int);
using IMG_Load_fptr = SDL_Surface* (*)(const char*);
using SDL_BlitSurface_fptr = int (*)(SDL_Surface*, const SDL_Rect*, SDL_Surface*, SDL_Rect*);
using SDL_UpdateTexture_fptr = int (*)(SDL_Texture*, const SDL_Rect*, const void*, int);
using SDL_LockTexture_fptr = int (*)(SDL_Texture*, const SDL_Rect*, void**, int*);
using SDL_UnlockTexture_fptr = void (*)(SDL_Texture*);
using SDL_SetTextureBlendMode_fptr = int (*)(SDL_Texture*, SDL_BlendMode);
using SDL_SetRenderDrawBlendMode_fptr = int (*)(SDL_Renderer*, SDL_BlendMode);
using SDL_GetWindowSize_fptr = int (*)(SDL_Window*, int*, int*);
using SDL_SetWindowSize_fptr = void (*)(SDL_Window*, int, int);
using SDL_SetWindowPosition_fptr = void (*)(SDL_Window*, int, int);
using SDL_GetWindowPosition_fptr = void (*)(SDL_Window*, int*, int*);
using SDL_SetWindowTitle_fptr = void (*)(SDL_Window*, const char*);
using SDL_GetWindowTitle_fptr = const char* (*)(SDL_Window*);
using SDL_ShowWindow_fptr = void (*)(SDL_Window*);
using SDL_HideWindow_fptr = void (*)(SDL_Window*);
using SDL_RaiseWindow_fptr = void (*)(SDL_Window*);
using SDL_MaximizeWindow_fptr = void (*)(SDL_Window*);
using SDL_MinimizeWindow_fptr = void (*)(SDL_Window*);
using SDL_RestoreWindow_fptr = void (*)(SDL_Window*);
using SDL_SetWindowFullscreen_fptr = int (*)(SDL_Window*, Uint32);
using SDL_GetWindowFlags_fptr = Uint32 (*)(SDL_Window*);
using SDL_GL_SetAttribute_fptr = int (*)(SDL_GLattr, int);
using SDL_GL_GetAttribute_fptr = int (*)(SDL_GLattr, int*);
using SDL_GL_SwapWindow_fptr = void (*)(SDL_Window*);
using SDL_GL_SetSwapInterval_fptr = int (*)(int);
using SDL_GL_GetSwapInterval_fptr = int (*)(void);
using SDL_GL_DeleteContext_fptr = void (*)(SDL_GLContext);
using SDL_GL_MakeCurrent_fptr = int (*)(SDL_Window*, SDL_GLContext);
using SDL_GL_GetProcAddress_fptr = void* (*)(const char*);
using SDL_GetNumVideoDisplays_fptr = int (*)(void);
using SDL_GetDisplayBounds_fptr = int (*)(int, SDL_Rect*);
using SDL_GetDesktopDisplayMode_fptr = int (*)(int, SDL_DisplayMode*);
using SDL_GetCurrentDisplayMode_fptr = int (*)(int, SDL_DisplayMode*);
using SDL_GetMouseState_fptr = Uint8 (*)(int*, int*);
using SDL_ShowCursor_fptr = int (*)(int);
using SDL_CreateSystemCursor_fptr = SDL_Cursor* (*)(SDL_SystemCursor);
using SDL_FreeCursor_fptr = void (*)(SDL_Cursor*);
using SDL_SetCursor_fptr = void (*)(SDL_Cursor*);
using SDL_GetCursor_fptr = SDL_Cursor* (*)(void);
using SDL_WarpMouseInWindow_fptr = int (*)(SDL_Window*, int, int);
using SDL_SetRelativeMouseMode_fptr = int (*)(SDL_bool);
using SDL_GetRelativeMouseMode_fptr = SDL_bool (*)(void);
using SDL_GetKeyboardState_fptr = const Uint8* (*)(int*);
using SDL_GetModState_fptr = SDL_Keymod (*)(void);
using SDL_SetModState_fptr = void (*)(SDL_Keymod);
using SDL_GetKeyName_fptr = const char* (*)(SDL_Keycode);
using SDL_GetKeyFromName_fptr = SDL_Keycode (*)(const char*);
using SDL_StartTextInput_fptr = void (*)(void);
using SDL_StopTextInput_fptr = void (*)(void);
using SDL_IsTextInputActive_fptr = SDL_bool (*)(void);

void* real_dlsym_bootstrap_sdl(void* handle, const char* symbol);