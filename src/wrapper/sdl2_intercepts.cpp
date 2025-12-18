#include "sdl2_intercepts.h"


#define SDL2HOOK_FUNCTION(ret_type, func_name, params, param_names, debug_fmt, ...) \
  ret_type func_name params { \
    static func_name##_fptr real = nullptr; \
    if (!real) { \
      real = (func_name##_fptr)real_dlsym(RTLD_NEXT, #func_name); \
      printf_if_debug("[HOOK] " #func_name " real function resolved to %p\n", real); \
      if (!real) { \
        printf_if_debug("[HOOK] FAILED TO RESOLVE FUNCTION!"); \
        exit(1); \
      } \
    } \
    printf_if_debug("[HOOK] " #func_name "(" debug_fmt ")\n", ##__VA_ARGS__); \
    if constexpr (std::is_void_v<ret_type>) { \
      real param_names; \
    } else { \
      return real param_names; \
    } \
  }

/* ---------- SDL2 Core Functions ---------- */

extern "C" {

  SDL2HOOK_FUNCTION(int, SDL_Init, (Uint32 flags), (flags), "flags=0x%x", flags)
  SDL2HOOK_FUNCTION(void, SDL_Quit, (void), (), "")

  SDL2HOOK_FUNCTION(SDL_Window*, SDL_CreateWindow, (const char* title, int x, int y, int w, int h, Uint32 flags), (title, x, y, w, h, flags), "title=%s, x=%d, y=%d, w=%d, h=%d, flags=0x%x", title ? title : "NULL", x, y, w, h, flags)
  SDL2HOOK_FUNCTION(void, SDL_DestroyWindow, (SDL_Window* window), (window), "window=%p", window)
  SDL2HOOK_FUNCTION(int, SDL_GetWindowSize, (SDL_Window* window, int* w, int* h), (window, w, h), "window=%p", window)
  SDL2HOOK_FUNCTION(void, SDL_SetWindowSize, (SDL_Window* window, int w, int h), (window, w, h), "window=%p, w=%d, h=%d", window, w, h)
  SDL2HOOK_FUNCTION(void, SDL_SetWindowPosition, (SDL_Window* window, int x, int y), (window, x, y), "window=%p, x=%d, y=%d", window, x, y)
  SDL2HOOK_FUNCTION(void, SDL_GetWindowPosition, (SDL_Window* window, int* x, int* y), (window, x, y), "window=%p", window)
  SDL2HOOK_FUNCTION(void, SDL_SetWindowTitle, (SDL_Window* window, const char* title), (window, title), "window=%p, title=%s", window, title ? title : "NULL")
  SDL2HOOK_FUNCTION(const char*, SDL_GetWindowTitle, (SDL_Window* window), (window), "window=%p", window)
  SDL2HOOK_FUNCTION(void, SDL_ShowWindow, (SDL_Window* window), (window), "window=%p", window)
  SDL2HOOK_FUNCTION(void, SDL_HideWindow, (SDL_Window* window), (window), "window=%p", window)
  SDL2HOOK_FUNCTION(void, SDL_RaiseWindow, (SDL_Window* window), (window), "window=%p", window)
  SDL2HOOK_FUNCTION(void, SDL_MaximizeWindow, (SDL_Window* window), (window), "window=%p", window)
  SDL2HOOK_FUNCTION(void, SDL_MinimizeWindow, (SDL_Window* window), (window), "window=%p", window)
  SDL2HOOK_FUNCTION(void, SDL_RestoreWindow, (SDL_Window* window), (window), "window=%p", window)
  SDL2HOOK_FUNCTION(int, SDL_SetWindowFullscreen, (SDL_Window* window, Uint32 flags), (window, flags), "window=%p, flags=0x%x", window, flags)
  SDL2HOOK_FUNCTION(Uint32, SDL_GetWindowFlags, (SDL_Window* window), (window), "window=%p", window)

  /* ---------- Renderer Management ---------- */

  SDL2HOOK_FUNCTION(SDL_Renderer*, SDL_CreateRenderer, (SDL_Window* window, int index, Uint32 flags), (window, index, flags), "window=%p, index=%d, flags=0x%x", window, index, flags)
  SDL2HOOK_FUNCTION(void, SDL_DestroyRenderer, (SDL_Renderer* renderer), (renderer), "renderer=%p", renderer)
  SDL2HOOK_FUNCTION(int, SDL_RenderClear, (SDL_Renderer* renderer), (renderer), "renderer=%p", renderer)
  SDL2HOOK_FUNCTION(int, SDL_RenderCopy, (SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect* srcrect, const SDL_Rect* dstrect), (renderer, texture, srcrect, dstrect), "renderer=%p, texture=%p, srcrect=%p, dstrect=%p", renderer, texture, srcrect, dstrect)
  SDL2HOOK_FUNCTION(void, SDL_RenderPresent, (SDL_Renderer* renderer), (renderer), "renderer=%p", renderer)
  SDL2HOOK_FUNCTION(int, SDL_SetRenderDrawColor, (SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a), (renderer, r, g, b, a), "renderer=%p, r=%u, g=%u, b=%u, a=%u", renderer, r, g, b, a)
  SDL2HOOK_FUNCTION(int, SDL_RenderDrawPoint, (SDL_Renderer* renderer, int x, int y), (renderer, x, y), "renderer=%p, x=%d, y=%d", renderer, x, y)
  SDL2HOOK_FUNCTION(int, SDL_RenderDrawLine, (SDL_Renderer* renderer, int x1, int y1, int x2, int y2), (renderer, x1, y1, x2, y2), "renderer=%p, x1=%d, y1=%d, x2=%d, y2=%d", renderer, x1, y1, x2, y2)
  SDL2HOOK_FUNCTION(int, SDL_RenderDrawRect, (SDL_Renderer* renderer, const SDL_Rect* rect), (renderer, rect), "renderer=%p, rect=%p", renderer, rect)
  SDL2HOOK_FUNCTION(int, SDL_RenderFillRect, (SDL_Renderer* renderer, const SDL_Rect* rect), (renderer, rect), "renderer=%p, rect=%p", renderer, rect)
  SDL2HOOK_FUNCTION(int, SDL_SetRenderDrawBlendMode, (SDL_Renderer* renderer, SDL_BlendMode blendMode), (renderer, blendMode), "renderer=%p, blendMode=%d", renderer, blendMode)

  /* ---------- Surface Management ---------- */
  SDL2HOOK_FUNCTION(SDL_Surface*, SDL_CreateRGBSurface, (Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask), (flags, width, height, depth, Rmask, Gmask, Bmask, Amask), "flags=0x%x, w=%d, h=%d, depth=%d", flags, width, height, depth)
  SDL2HOOK_FUNCTION(void, SDL_FreeSurface, (SDL_Surface* surface), (surface), "surface=%p", surface)
  SDL2HOOK_FUNCTION(SDL_Surface*, SDL_LoadBMP_RW, (SDL_RWops* src, int freesrc), (src, freesrc), "src=%p, freesrc=%d", src, freesrc)
  SDL2HOOK_FUNCTION(int, SDL_SaveBMP_RW, (SDL_Surface* surface, SDL_RWops* dst, int freedst), (surface, dst, freedst), "surface=%p, dst=%p, freedst=%d", surface, dst, freedst)

  // SDL_BlitSurface custom handling
  int SDL_BlitSurface(SDL_Surface* src, const SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect) {
    static SDL_BlitSurface_fptr real = nullptr;
    if (!real) real = (SDL_BlitSurface_fptr)real_dlsym_bootstrap_sdl(RTLD_NEXT, "SDL_UpperBlit");
    printf_if_debug("[HOOK] SDL_BlitSurface(src=%p, srcrect=%p, dst=%p, dstrect=%p)\n", src, srcrect, dst, dstrect);
    return real ? real(src, srcrect, dst, dstrect) : -1;
  }

  /* ---------- Texture Management ---------- */

  SDL2HOOK_FUNCTION(SDL_Texture*, SDL_CreateTexture, (SDL_Renderer* renderer, Uint32 format, int access, int w, int h), (renderer, format, access, w, h), "renderer=%p, format=0x%x, access=%d, w=%d, h=%d", renderer, format, access, w, h)
  SDL2HOOK_FUNCTION(SDL_Texture*, SDL_CreateTextureFromSurface, (SDL_Renderer* renderer, SDL_Surface* surface), (renderer, surface), "renderer=%p, surface=%p", renderer, surface)
  SDL2HOOK_FUNCTION(void, SDL_DestroyTexture, (SDL_Texture* texture), (texture), "texture=%p", texture)
  SDL2HOOK_FUNCTION(int, SDL_UpdateTexture, (SDL_Texture* texture, const SDL_Rect* rect, const void* pixels, int pitch), (texture, rect, pixels, pitch), "texture=%p, rect=%p, pixels=%p, pitch=%d", texture, rect, pixels, pitch)
  SDL2HOOK_FUNCTION(int, SDL_LockTexture, (SDL_Texture* texture, const SDL_Rect* rect, void** pixels, int* pitch), (texture, rect, pixels, pitch), "texture=%p, rect=%p", texture, rect)
  SDL2HOOK_FUNCTION(void, SDL_UnlockTexture, (SDL_Texture* texture), (texture), "texture=%p", texture)
  SDL2HOOK_FUNCTION(int, SDL_SetTextureBlendMode, (SDL_Texture* texture, SDL_BlendMode blendMode), (texture, blendMode), "texture=%p, blendMode=%d", texture, blendMode)
  SDL2HOOK_FUNCTION(int, SDL_PollEvent, (SDL_Event* event), (event), "")
  SDL2HOOK_FUNCTION(int, SDL_WaitEvent, (SDL_Event* event), (event), "")
  SDL2HOOK_FUNCTION(int, SDL_PushEvent, (SDL_Event* event), (event), "type=0x%x", event ? event->type : 0)
  SDL2HOOK_FUNCTION(Uint32, SDL_GetTicks, (void), (), "")
  SDL2HOOK_FUNCTION(Uint64, SDL_GetPerformanceCounter, (void), (), "")
  SDL2HOOK_FUNCTION(Uint64, SDL_GetPerformanceFrequency, (void), (), "")
  SDL2HOOK_FUNCTION(void, SDL_Delay, (Uint32 ms), (ms), "ms=%u", ms)
  SDL2HOOK_FUNCTION(int, SDL_GL_SetAttribute, (SDL_GLattr attr, int value), (attr, value), "attr=%d, value=%d", attr, value)
  SDL2HOOK_FUNCTION(int, SDL_GL_GetAttribute, (SDL_GLattr attr, int* value), (attr, value), "attr=%d", attr)
  SDL2HOOK_FUNCTION(void, SDL_GL_SwapWindow, (SDL_Window* window), (window), "window=%p", window)
  SDL2HOOK_FUNCTION(int, SDL_GL_SetSwapInterval, (int interval), (interval), "interval=%d", interval)
  SDL2HOOK_FUNCTION(int, SDL_GL_GetSwapInterval, (void), (), "")
  SDL2HOOK_FUNCTION(void, SDL_GL_DeleteContext, (SDL_GLContext context), (context), "context=%p", context)
  SDL2HOOK_FUNCTION(int, SDL_GL_MakeCurrent, (SDL_Window* window, SDL_GLContext context), (window, context), "window=%p, context=%p", window, context)

  // SDL_GL_GetProcAddress custom handling
  void* SDL_GL_GetProcAddress(const char* proc) {
    static SDL_GL_GetProcAddress_fptr real = nullptr;
    if (!real) real = (SDL_GL_GetProcAddress_fptr)real_dlsym_bootstrap_sdl(RTLD_NEXT, "SDL_GL_GetProcAddress");
    printf_if_debug("[HOOK] SDL_GL_GetProcAddress(proc=%s)\n", proc ? proc : "NULL");
    return real ? real(proc) : nullptr;
  }

  SDL2HOOK_FUNCTION(int, SDL_GetNumVideoDisplays, (void), (), "")
  SDL2HOOK_FUNCTION(int, SDL_GetDisplayBounds, (int displayIndex, SDL_Rect* rect), (displayIndex, rect), "displayIndex=%d", displayIndex)
  SDL2HOOK_FUNCTION(int, SDL_GetDesktopDisplayMode, (int displayIndex, SDL_DisplayMode* mode), (displayIndex, mode), "displayIndex=%d", displayIndex)
  SDL2HOOK_FUNCTION(int, SDL_GetCurrentDisplayMode, (int displayIndex, SDL_DisplayMode* mode), (displayIndex, mode), "displayIndex=%d", displayIndex)

  SDL2HOOK_FUNCTION(Uint8, SDL_GetMouseState, (int* x, int* y), (x, y), "")
  SDL2HOOK_FUNCTION(int, SDL_ShowCursor, (int toggle), (toggle), "toggle=%d", toggle)
  SDL2HOOK_FUNCTION(SDL_Cursor*, SDL_CreateSystemCursor, (SDL_SystemCursor id), (id), "id=%d", id)
  SDL2HOOK_FUNCTION(void, SDL_FreeCursor, (SDL_Cursor* cursor), (cursor), "cursor=%p", cursor)
  SDL2HOOK_FUNCTION(void, SDL_SetCursor, (SDL_Cursor* cursor), (cursor), "cursor=%p", cursor)
  SDL2HOOK_FUNCTION(SDL_Cursor*, SDL_GetCursor, (void), (), "")
  SDL2HOOK_FUNCTION(int, SDL_WarpMouseInWindow, (SDL_Window* window, int x, int y), (window, x, y), "window=%p, x=%d, y=%d", window, x, y)
  SDL2HOOK_FUNCTION(int, SDL_SetRelativeMouseMode, (SDL_bool enabled), (enabled), "enabled=%d", enabled)
  SDL2HOOK_FUNCTION(SDL_bool, SDL_GetRelativeMouseMode, (void), (), "")

  SDL2HOOK_FUNCTION(const Uint8*, SDL_GetKeyboardState, (int* numkeys), (numkeys), "")
  SDL2HOOK_FUNCTION(SDL_Keymod, SDL_GetModState, (void), (), "")
  SDL2HOOK_FUNCTION(void, SDL_SetModState, (SDL_Keymod modstate), (modstate), "modstate=%d", modstate)
  SDL2HOOK_FUNCTION(const char*, SDL_GetKeyName, (SDL_Keycode key), (key), "key=%d", key)
  SDL2HOOK_FUNCTION(SDL_Keycode, SDL_GetKeyFromName, (const char* name), (name), "name=%s", name ? name : "NULL")
  SDL2HOOK_FUNCTION(void, SDL_StartTextInput, (void), (), "")
  SDL2HOOK_FUNCTION(void, SDL_StopTextInput, (void), (), "")
  SDL2HOOK_FUNCTION(SDL_bool, SDL_IsTextInputActive, (void), (), "")

  // SDL_GLContext SDL_GL_CreateContext(SDL_Window* window) {

  //   static SDL_GL_CreateContext_t real_fn =
  //   reinterpret_cast<SDL_GL_CreateContext_t>(dlsym(RTLD_NEXT, "SDL_GL_CreateContext"));

  //   if (!real_fn) {
  //     printf_if_debug("[HOOK] ERROR: Could not resolve SDL_GL_CreateContext!\n");
  //     return nullptr;
  //   }

  //   printf_if_debug("[HOOK] SDL_GL_CreateContext() called, window ptr = %p\n", window);

  //   using Context = SDL_GLContext;
  //   using Window  = SDL_Window*;

  //   Context ctx = real_fn(static_cast<Window>(window));

  //   if (ctx) {
  //     printf_if_debug("[HOOK] SDL_GL_CreateContext() succeeded, context = %p\n", ctx);
  //   } else {
  //     printf_if_debug("[HOOK] SDL_GL_CreateContext() failed!\n");
  //   }

  //   return ctx;
  // }


}