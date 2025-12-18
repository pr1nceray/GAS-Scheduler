#pragma once
#define GL_GLEXT_PROTOTYPES 0  
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <X11/Xutil.h>

struct __GLXVisualInfo {};  
using GLXVisualInfo = __GLXVisualInfo;

using GLXContext = void*;
using GLXFBConfig = void*;
