#include "opengl_intercepts.h"

struct gl_func_wrapper_s {
  void ** real_ptr;
  void * wrapper_ptr;
};

std::unordered_map<std::string, gl_func_wrapper_s> gl_func_registry;

constexpr std::array gpu_draw_calls = {
  std::string_view("glDrawElements"),
  std::string_view("glDrawArrays"),
  std::string_view("glDrawElementsInstanced"),
  std::string_view("glDrawElementsBaseVertex"),
  std::string_view("glDrawRangeElements"),
  std::string_view("glMultiDrawElementsIndirect"),
  std::string_view("glDrawElementsIndirect"),
  std::string_view("glClear"),
  std::string_view("glEnd"),  
  std::string_view("glCallList") 
};

constexpr std::array gpu_frame_boundaries = {
  "glXSwapBuffers",
  "glFinish",
  "glFlush"
};

constexpr bool is_frame_boundary(std::string_view name) {
  return (std::ranges::find(gpu_frame_boundaries, name) != gpu_frame_boundaries.end());
}

constexpr bool is_gpu_call(std::string_view name) {
  return (std::ranges::find(gpu_draw_calls, name) != gpu_draw_calls.end());
}

#define GL_HOOK_REGISTER(func_name) \
namespace { \
  struct func_name##_registrar { \
    func_name##_registrar() { \
      gl_func_registry[#func_name] = gl_func_wrapper_s { \
        reinterpret_cast<void**>(&func_name##_fptr_resolved), \
        reinterpret_cast<void*>(&func_name) \
      }; \
    } \
  } func_name##_reg_instance; \
}
// create namespace to create non conflicting function 
// to insert function into map and have it map to a corresponding void **
// and wrapped function

float frames_last_second = 0;
std::chrono::steady_clock::time_point time_taken {};

#define GL_HOOK_WRAPPER(ret_type, func_name, params, param_names, debug_fmt, ...) \
func_name##_fptr func_name##_fptr_resolved = nullptr; \
ret_type func_name params { \
  /*any printfs nukes our framerates, though this is good for debugging*/ \
  printf_if_debug("[HOOK] " #func_name "(" debug_fmt ")\n", ##__VA_ARGS__); \
  if (!func_name##_fptr_resolved) { \
    printf_if_debug("[HOOK] resolving function name....\n"); \
    func_name##_fptr_resolved = reinterpret_cast<func_name##_fptr>(real_dlsym(RTLD_NEXT, #func_name)); \
    if (func_name##_fptr_resolved) { \
      printf_if_debug("[HOOK] " #func_name " real function resolved to %p\n", func_name##_fptr_resolved); \
    } else { \
      printf_if_debug("[HOOK] FAILED TO RESOLVE FUNCTION!"); \
      exit(1); \
    } \
  } \
  \
  send_message_socket_if_debug(interceptor_daemon::operation_reqs::DEBUG_PRINT, "calling " #func_name); \
  if constexpr (is_frame_boundary(std::string_view(#func_name))) { \
    /*printf_if_debug("[HOOK] calling frame boundary\n");*/ \
    \
    printf_if_debug("[HOOK] message id being sent = %lu\n", message_id); \
    frames_last_second = (frames_last_second + 1); \
    std::chrono::steady_clock::duration time_since_fps_update = std::chrono::steady_clock::now() - time_taken; \
    if (time_since_fps_update  >= std::chrono::milliseconds(500)) { \
      if (time_since_fps_update <= std::chrono::milliseconds(1000)) { \
        float miliseconds_update = std::chrono::duration<float, std::milli>(time_since_fps_update).count(); \
        float actual_fps = frames_last_second / (miliseconds_update / 1000.0f); \
        send_message_socket(interceptor_daemon::operation_reqs::FRAMERATE_PING, #func_name, actual_fps);\
      } \
      time_taken = std::chrono::steady_clock::now(); \
      frames_last_second = 0; \
    } \
    sleep_var->store(-1, std::memory_order_release); \
    send_message_socket(interceptor_daemon::operation_reqs::REQUEST, "\0"); \
    int you_spin_me_right_round = 0; \
    while (true) { \
      you_spin_me_right_round = sleep_var->load(std::memory_order_acquire);\
      if (you_spin_me_right_round != -1) { \
        break; \
      } \
      if (!ring_buff->ready.load(std::memory_order_acquire)) { \
        printf_if_debug("could not connect - is daemon ready?\n"); \
        exit(1); \
      } \
      _mm_pause(); \
    } \
    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now(); \
    \
    if (you_spin_me_right_round > 0) {\
      while ((std::chrono::steady_clock::now() - start_time) < std::chrono::microseconds(you_spin_me_right_round)) {  \
        _mm_pause(); \
      } \
    } \
    \
  } \
  \
  if constexpr (std::is_void_v<ret_type>) { \
    func_name##_fptr_resolved param_names; \
  } else { \
    return func_name##_fptr_resolved param_names; \
  } \
} \
GL_HOOK_REGISTER(func_name) \

#define IMPL_GLX_GET_PROC_ADDRESS(func_name, return_type) \
return_type func_name(const GLubyte *proc_name) { \
  if (proc_name) { \
    printf_if_debug("[HOOK] " #func_name "(%s)\n", (const char*)proc_name); \
  } else { \
    printf_if_debug("[HOOK] " #func_name " called with bad proc_name\n"); \
    exit(1); \
  } \
  if (!func_name##_fptr_resolved) { \
    func_name##_fptr_resolved = reinterpret_cast<func_name##_fptr>( \
      real_dlsym(RTLD_NEXT, #func_name) \
    ); \
    printf_if_debug("[HOOK] " #func_name " real function resolved to %p\n", func_name##_fptr_resolved); \
  } \
  if(!func_name##_fptr_resolved) { \
    printf_if_debug("[HOOK] failed to resolve " #func_name ", exiting\n"); \
    exit(1); \
  } \
  auto iter = gl_func_registry.find(std::string((const char *)proc_name)); \
  if (iter == gl_func_registry.end()) { \
    printf_if_debug("failed to resolve " #func_name " symbol %s\n", proc_name); \
    return func_name##_fptr_resolved ? reinterpret_cast<return_type>(func_name##_fptr_resolved(proc_name)) : nullptr; \
  } \
  if (*iter->second.real_ptr == nullptr) { \
    *iter->second.real_ptr = reinterpret_cast<void **>( \
      func_name##_fptr_resolved(proc_name) \
    ); \
  } \
  return reinterpret_cast<return_type>(iter->second.wrapper_ptr); \
} \
GL_HOOK_REGISTER(func_name) \







extern "C" {
  //glxgetprocaddress wrappers
  IMPL_GLX_GET_PROC_ADDRESS(glXGetProcAddress, void*)
  IMPL_GLX_GET_PROC_ADDRESS(glXGetProcAddressARB, __GLXextFuncPtr)

  GL_HOOK_WRAPPER(void, glDrawElements, (GLenum mode, GLsizei count, GLenum type, const void *indices), (mode, count, type, indices), "mode=0x%x, count=%d, type=0x%x, indices=%p", mode, count, type, indices)
  GL_HOOK_WRAPPER(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count), (mode, first, count), "mode=0x%x, first=%d, count=%d", mode, first, count)
  // GL_HOOK_WRAPPER(void, glUseProgram, (GLuint program), (program), "program=%u", program)
  // GL_HOOK_WRAPPER(void, glBindBuffer, (GLenum target, GLuint buffer), (target, buffer), "target=0x%x, buffer=%u", target, buffer)
  // GL_HOOK_WRAPPER(GLuint, glCreateShader, (GLenum type), (type), "type=0x%x", type)
  // GL_HOOK_WRAPPER(void, glCompileShader, (GLuint shader), (shader), "shader=%u", shader)
  // GL_HOOK_WRAPPER(void, glDrawElementsInstanced, (GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount), (mode, count, type, indices, instancecount), "mode=%u, count=%d, type=%u, indices=%p, instancecount=%d", mode, count, type, indices, instancecount)
  // GL_HOOK_WRAPPER(void, glBufferData, (GLenum target, GLsizeiptr size, const void *data, GLenum usage), (target, size, data, usage), "target=0x%x, size=%zd, data=%p, usage=0x%x", target, size, data, usage)

  // GL_HOOK_WRAPPER(void, glShaderSource, (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length), (shader, count, string, length), "shader=%u, count=%d, string=%p, length=%p", shader, count, string, length)

  // GL_HOOK_WRAPPER(void, glDrawElementsBaseVertex, (GLenum mode, GLsizei count, GLenum type, const void* indices, GLint basevertex), (mode, count, type, indices, basevertex), "mode=%u, count=%d, type=%u, indices=%p, basevertex=%d", mode, count, type, indices, basevertex)
  // GL_HOOK_WRAPPER(void, glDrawRangeElements, (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices), (mode, start, end, count, type, indices), "mode=%u, start=%u, end=%u, count=%d, type=%u, indices=%p", mode, start, end, count, type, indices)
  // GL_HOOK_WRAPPER(void, glMultiDrawElementsIndirect, (GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride), (mode, type, indirect, drawcount, stride), "mode=%u, type=%u, indirect=%p, drawcount=%d, stride=%d", mode, type, indirect, drawcount, stride)
  // GL_HOOK_WRAPPER(void, glDrawElementsIndirect, (GLenum mode, GLenum type, const void* indirect), (mode, type, indirect), "mode=%u, type=%u, indirect=%p", mode, type, indirect)

  GL_HOOK_WRAPPER(void, glClear, (GLuint mask), (mask), "mask=0x%x", mask)
  GL_HOOK_WRAPPER(void, glBegin, (GLenum mode), (mode), "mode=0x%x", mode)
  GL_HOOK_WRAPPER(void, glEnd, (void), (), "", "")
  // GL_HOOK_WRAPPER(void, glPushMatrix, (void), (), "", "")
  // GL_HOOK_WRAPPER(void, glPopMatrix, (void), (), "", "")
  // GL_HOOK_WRAPPER(void, glRotatef, (float angle, float x, float y, float z), (angle, x, y, z), "angle=%.2f, x=%.2f, y=%.2f, z=%.2f", angle, x, y, z)
  // GL_HOOK_WRAPPER(void, glTranslatef, (float x, float y, float z), (x, y, z), "x=%.2f, y=%.2f, z=%.2f", x, y, z)
  // GL_HOOK_WRAPPER(void, glCallList, (unsigned int list), (list), "list=%u", list)
  // GL_HOOK_WRAPPER(void, glNormal3f, (float x, float y, float z), (x, y, z), "x=%.2f, y=%.2f, z=%.2f", x, y, z)
  // GL_HOOK_WRAPPER(void, glShadeModel, (unsigned int mode), (mode), "mode=%u", mode)
  // GL_HOOK_WRAPPER(void, glVertex3f, (float x, float y, float z), (x, y, z), "x=%.2f, y=%.2f, z=%.2f", x, y, z)
  GL_HOOK_WRAPPER(void, glXSwapBuffers, (void* dpy, void* drawable), (dpy, drawable), "dpy=%p, drawable=%p", dpy, drawable)
  // GL_HOOK_WRAPPER(GLXContext, glXGetCurrentContext, (void), (), "", "")
  // GL_HOOK_WRAPPER(GLXDrawable, glXGetCurrentDrawable, (void), (), "", "")
  // GL_HOOK_WRAPPER(const char*, glXQueryExtensionsString, (Display* dpy, int screen), (dpy, screen), "dpy=%p, screen=%d", dpy, screen)
  // GL_HOOK_WRAPPER(void, glXSwapIntervalEXT, (Display* dpy, GLXDrawable drawable, int interval), (dpy, drawable, interval), "dpy=%p, drawable=%lu, interval=%d", dpy, drawable, interval)
  // GL_HOOK_WRAPPER(int, glXSwapIntervalSGI, (int interval), (interval), "interval=%d", interval)
  // GL_HOOK_WRAPPER(GLXContext, glXCreateContextAttribsARB, (Display* dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int* attrib_list), (dpy, config, share_context, direct, attrib_list), "dpy=%p, config=%p, share_context=%p, direct=%d, attrib_list=%p", dpy, config, share_context, direct, attrib_list)
  // GL_HOOK_WRAPPER(GLXFBConfig*, glXChooseFBConfig, (Display* dpy, int screen, const int* attrib_list, int* nelements), (dpy, screen, attrib_list, nelements), "dpy=%p, screen=%d, attrib_list=%p, nelements=%p", dpy, screen, attrib_list, nelements)
  // GL_HOOK_WRAPPER(const GLubyte*, glGetString, (GLenum name), (name), "name=0x%x", name)
  // GL_HOOK_WRAPPER(const GLubyte*, glGetStringi, (GLenum name, GLuint index), (name, index), "name=0x%x, index=%u", name, index)
  // GL_HOOK_WRAPPER(void, glGetIntegerv, (GLenum pname, GLint* data), (pname, data), "pname=0x%x, data=%p", pname, data)
  // GL_HOOK_WRAPPER(XVisualInfo*, glXChooseVisual, (Display* dpy, int screen, int* attribList), (dpy, screen, attribList), "dpy=%p, screen=%d, attribList=%p", dpy, screen, attribList)
  // GL_HOOK_WRAPPER(GLXContext, glXCreateContext, (Display* dpy, XVisualInfo* vis, GLXContext shareList, Bool direct), (dpy, vis, shareList, direct), "dpy=%p, vis=%p, shareList=%p, direct=%d", dpy, vis, shareList, direct)
  // GL_HOOK_WRAPPER(void, glXDestroyContext, (Display* dpy, GLXContext ctx), (dpy, ctx), "dpy=%p, ctx=%p", dpy, ctx)
  // GL_HOOK_WRAPPER(Bool, glXMakeCurrent, (Display* dpy, GLXDrawable drawable, GLXContext ctx), (dpy, drawable, ctx), "dpy=%p, drawable=%lu, ctx=%p", dpy, drawable, ctx)
  // GL_HOOK_WRAPPER(void, glXQueryDrawable, (Display* dpy, GLXDrawable draw, int attribute, unsigned int* value), (dpy, draw, attribute, value), "dpy=%p, draw=%lu, attribute=%d, value=%p", dpy, draw, attribute, value)
  
  GL_HOOK_WRAPPER(void, glFinish, (void), (), "")
  GL_HOOK_WRAPPER(void, glFlush, (void), (), "")

  // GL_HOOK_WRAPPER(void*, glXGetVisualFromFBConfig, (Display* dpy, GLXFBConfig config), (dpy, config), "dpy=%p, config=%p", dpy, config)
  // GL_HOOK_WRAPPER(void, glActiveTexture, (GLenum texture), (texture), "texture=0x%x", texture)
  // GL_HOOK_WRAPPER(void, glAttachShader, (GLuint program, GLuint shader), (program, shader), "program=%u, shader=%u", program, shader)
  // GL_HOOK_WRAPPER(void, glBindAttribLocation, (GLuint program, GLuint index, const GLchar* name), (program, index, name), "program=%u, index=%u, name=%s", program, index, name)
  // GL_HOOK_WRAPPER(void, glBindFramebuffer, (GLenum target, GLuint framebuffer), (target, framebuffer), "target=0x%x, framebuffer=%u", target, framebuffer)
  // GL_HOOK_WRAPPER(void, glBindRenderbuffer, (GLenum target, GLuint renderbuffer), (target, renderbuffer), "target=0x%x, renderbuffer=%u", target, renderbuffer)
  // GL_HOOK_WRAPPER(void, glBindTexture, (GLenum target, GLuint texture), (target, texture), "target=0x%x, texture=%u", target, texture)
  // GL_HOOK_WRAPPER(void, glBlendEquation, (GLenum mode), (mode), "mode=0x%x", mode)
  // GL_HOOK_WRAPPER(void, glBlendEquationSeparate, (GLenum modeRGB, GLenum modeAlpha), (modeRGB, modeAlpha), "modeRGB=0x%x, modeAlpha=0x%x", modeRGB, modeAlpha)
  // GL_HOOK_WRAPPER(void, glBlendFuncSeparate, (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha), (srcRGB, dstRGB, srcAlpha, dstAlpha), "srcRGB=0x%x, dstRGB=0x%x, srcAlpha=0x%x, dstAlpha=0x%x", srcRGB, dstRGB, srcAlpha, dstAlpha)
  // GL_HOOK_WRAPPER(void, glBufferSubData, (GLenum target, GLintptr offset, GLsizeiptr size, const void* data), (target, offset, size, data), "target=0x%x, offset=%ld, size=%ld, data=%p", target, offset, size, data)
  // GL_HOOK_WRAPPER(GLenum, glCheckFramebufferStatus, (GLenum target), (target), "target=0x%x", target)
  // GL_HOOK_WRAPPER(void, glClearColor, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha), (red, green, blue, alpha), "red=%.2f, green=%.2f, blue=%.2f, alpha=%.2f", red, green, blue, alpha)
  // GL_HOOK_WRAPPER(void, glClearDepthf, (GLfloat depth), (depth), "depth=%.2f", depth)
  // GL_HOOK_WRAPPER(void, glClearStencil, (GLint s), (s), "s=%d", s)
  // GL_HOOK_WRAPPER(void, glColorMask, (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha), (red, green, blue, alpha), "red=%d, green=%d, blue=%d, alpha=%d", red, green, blue, alpha)
  // GL_HOOK_WRAPPER(void, glCompressedTexImage2D, (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data), (target, level, internalformat, width, height, border, imageSize, data), "target=0x%x, level=%d, internalformat=0x%x, width=%d, height=%d, border=%d, imageSize=%d, data=%p", target, level, internalformat, width, height, border, imageSize, data)
  // GL_HOOK_WRAPPER(void, glCompressedTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data), (target, level, xoffset, yoffset, width, height, format, imageSize, data), "target=0x%x, level=%d, xoffset=%d, yoffset=%d, width=%d, height=%d, format=0x%x, imageSize=%d, data=%p", target, level, xoffset, yoffset, width, height, format, imageSize, data)
  // GL_HOOK_WRAPPER(void, glCopyTexImage2D, (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border), (target, level, internalformat, x, y, width, height, border), "target=0x%x, level=%d, internalformat=0x%x, x=%d, y=%d, width=%d, height=%d, border=%d", target, level, internalformat, x, y, width, height, border)
  // GL_HOOK_WRAPPER(void, glCopyTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height), (target, level, xoffset, yoffset, x, y, width, height), "target=0x%x, level=%d, xoffset=%d, yoffset=%d, x=%d, y=%d, width=%d, height=%d", target, level, xoffset, yoffset, x, y, width, height)
  // GL_HOOK_WRAPPER(GLuint, glCreateProgram, (void), (), "")
  // GL_HOOK_WRAPPER(void, glCullFace, (GLenum mode), (mode), "mode=0x%x", mode)
  // GL_HOOK_WRAPPER(void, glDeleteBuffers, (GLsizei n, const GLuint* buffers), (n, buffers), "n=%d, buffers=%p", n, buffers)
  // GL_HOOK_WRAPPER(void, glDeleteFramebuffers, (GLsizei n, const GLuint* framebuffers), (n, framebuffers), "n=%d, framebuffers=%p", n, framebuffers)
  // GL_HOOK_WRAPPER(void, glDeleteProgram, (GLuint program), (program), "program=%u", program)
  // GL_HOOK_WRAPPER(void, glDeleteRenderbuffers, (GLsizei n, const GLuint* renderbuffers), (n, renderbuffers), "n=%d, renderbuffers=%p", n, renderbuffers)
  // GL_HOOK_WRAPPER(void, glDeleteShader, (GLuint shader), (shader), "shader=%u", shader)
  // GL_HOOK_WRAPPER(void, glDeleteTextures, (GLsizei n, const GLuint* textures), (n, textures), "n=%d, textures=%p", n, textures)
  // GL_HOOK_WRAPPER(void, glDepthFunc, (GLenum func), (func), "func=0x%x", func)
  // GL_HOOK_WRAPPER(void, glDepthMask, (GLboolean flag), (flag), "flag=%d", flag)
  // GL_HOOK_WRAPPER(void, glDetachShader, (GLuint program, GLuint shader), (program, shader), "program=%u, shader=%u", program, shader)
  // GL_HOOK_WRAPPER(void, glDisable, (GLenum cap), (cap), "cap=0x%x", cap)
  // GL_HOOK_WRAPPER(void, glDisableVertexAttribArray, (GLuint index), (index), "index=%u", index)
  // GL_HOOK_WRAPPER(GLboolean, glIsEnabled, (GLenum cap), (cap), "cap=0x%x", cap)
  // GL_HOOK_WRAPPER(void, glEnable, (GLenum cap), (cap), "cap=0x%x", cap)
  // GL_HOOK_WRAPPER(void, glEnableVertexAttribArray, (GLuint index), (index), "index=%u", index)
  // GL_HOOK_WRAPPER(void, glFramebufferRenderbuffer, (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer), (target, attachment, renderbuffertarget, renderbuffer), "target=0x%x, attachment=0x%x, renderbuffertarget=0x%x, renderbuffer=%u", target, attachment, renderbuffertarget, renderbuffer)
  // GL_HOOK_WRAPPER(void, glGetRenderbufferParameteriv, (GLenum target, GLenum pname, GLint* params), (target, pname, params), "target=0x%x, pname=0x%x, params=%p", target, pname, params)
  // GL_HOOK_WRAPPER(void, glFramebufferTexture2D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level), (target, attachment, textarget, texture, level), "target=0x%x, attachment=0x%x, textarget=0x%x, texture=%u, level=%d", target, attachment, textarget, texture, level)
  // GL_HOOK_WRAPPER(void, glFramebufferTexture3D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer), (target, attachment, textarget, texture, level, layer), "target=0x%x, attachment=0x%x, textarget=0x%x, texture=%u, level=%d, layer=%d", target, attachment, textarget, texture, level, layer)
  // GL_HOOK_WRAPPER(void, glFrontFace, (GLenum mode), (mode), "mode=0x%x", mode)
  // GL_HOOK_WRAPPER(void, glGenBuffers, (GLsizei n, GLuint* buffers), (n, buffers), "n=%d, buffers=%p", n, buffers)
  // GL_HOOK_WRAPPER(void, glGenerateMipmap, (GLenum target), (target), "target=0x%x", target)
  // GL_HOOK_WRAPPER(void, glGenFramebuffers, (GLsizei n, GLuint* framebuffers), (n, framebuffers), "n=%d, framebuffers=%p", n, framebuffers)
  // GL_HOOK_WRAPPER(void, glGenRenderbuffers, (GLsizei n, GLuint* renderbuffers), (n, renderbuffers), "n=%d, renderbuffers=%p", n, renderbuffers)
  // GL_HOOK_WRAPPER(void, glGenTextures, (GLsizei n, GLuint* textures), (n, textures), "n=%d, textures=%p", n, textures)
  // GL_HOOK_WRAPPER(void, glGetActiveAttrib, (GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name), (program, index, bufSize, length, size, type, name), "program=%u, index=%u, bufSize=%d, length=%p, size=%p, type=%p, name=%p", program, index, bufSize, length, size, type, name)
  // GL_HOOK_WRAPPER(void, glGetActiveUniform, (GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name), (program, index, bufSize, length, size, type, name), "program=%u, index=%u, bufSize=%d, length=%p, size=%p, type=%p, name=%p", program, index, bufSize, length, size, type, name)
  // GL_HOOK_WRAPPER(GLint, glGetAttribLocation, (GLuint program, const GLchar* name), (program, name), "program=%u, name=%s", program, name)
  // GL_HOOK_WRAPPER(GLenum, glGetError, (void), (), "")
  // GL_HOOK_WRAPPER(void, glGetFramebufferAttachmentParameteriv, (GLenum target, GLenum attachment, GLenum pname, GLint* params), (target, attachment, pname, params), "target=0x%x, attachment=0x%x, pname=0x%x, params=%p", target, attachment, pname, params)
  // GL_HOOK_WRAPPER(void, glGetProgramiv, (GLuint program, GLenum pname, GLint* params), (program, pname, params), "program=%u, pname=0x%x, params=%p", program, pname, params)
  // GL_HOOK_WRAPPER(void, glGetProgramInfoLog, (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog), (program, bufSize, length, infoLog), "program=%u, bufSize=%d, length=%p, infoLog=%p", program, bufSize, length, infoLog)
  // GL_HOOK_WRAPPER(void, glValidateProgram, (GLuint program), (program), "program=%u", program)
  // GL_HOOK_WRAPPER(void, glGetShaderiv, (GLuint shader, GLenum pname, GLint* params), (shader, pname, params), "shader=%u, pname=0x%x, params=%p", shader, pname, params)
  // GL_HOOK_WRAPPER(void, glGetShaderSource, (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source), (shader, bufSize, length, source), "shader=%u, bufSize=%d, length=%p, source=%p", shader, bufSize, length, source)
  // GL_HOOK_WRAPPER(void, glGetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog), (shader, bufSize, length, infoLog), "shader=%u, bufSize=%d, length=%p, infoLog=%p", shader, bufSize, length, infoLog)
  // GL_HOOK_WRAPPER(void, glGetShaderPrecisionFormat, (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision), (shadertype, precisiontype, range, precision), "shadertype=0x%x, precisiontype=0x%x, range=%p, precision=%p", shadertype, precisiontype, range, precision)
  // GL_HOOK_WRAPPER(void, glGetTexParameteriv, (GLenum target, GLenum pname, GLint* params), (target, pname, params), "target=0x%x, pname=0x%x, params=%p", target, pname, params)
  // GL_HOOK_WRAPPER(void, glGetTexLevelParameterfv, (GLenum target, GLint level, GLenum pname, GLfloat* params), (target, level, pname, params), "target=0x%x, level=%d, pname=0x%x, params=%p", target, level, pname, params)
  // GL_HOOK_WRAPPER(void, glGetTexLevelParameteriv, (GLenum target, GLint level, GLenum pname, GLint* params), (target, level, pname, params), "target=0x%x, level=%d, pname=0x%x, params=%p", target, level, pname, params)
  // GL_HOOK_WRAPPER(void, glGetUniformiv, (GLuint program, GLint location, GLint* params), (program, location, params), "program=%u, location=%d, params=%p", program, location, params)
  // GL_HOOK_WRAPPER(GLint, glGetUniformLocation, (GLuint program, const GLchar* name), (program, name), "program=%u, name=%s", program, name)
  // GL_HOOK_WRAPPER(void, glGetVertexAttribiv, (GLuint index, GLenum pname, GLint* params), (index, pname, params), "index=%u, pname=0x%x, params=%p", index, pname, params)
  // GL_HOOK_WRAPPER(void, glLinkProgram, (GLuint program), (program), "program=%u", program)
  // GL_HOOK_WRAPPER(void, glPixelStorei, (GLenum pname, GLint param), (pname, param), "pname=0x%x, param=%d", pname, param)
  // GL_HOOK_WRAPPER(void, glPolygonOffset, (GLfloat factor, GLfloat units), (factor, units), "factor=%.2f, units=%.2f", factor, units)
  // GL_HOOK_WRAPPER(void, glReadPixels, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels), (x, y, width, height, format, type, pixels), "x=%d, y=%d, width=%d, height=%d, format=0x%x, type=0x%x, pixels=%p", x, y, width, height, format, type, pixels)
  // GL_HOOK_WRAPPER(void, glRenderbufferStorage, (GLenum target, GLenum internalformat, GLsizei width, GLsizei height), (target, internalformat, width, height), "target=0x%x, internalformat=0x%x, width=%d, height=%d", target, internalformat, width, height)
  // GL_HOOK_WRAPPER(void, glScissor, (GLint x, GLint y, GLsizei width, GLsizei height), (x, y, width, height), "x=%d, y=%d, width=%d, height=%d", x, y, width, height)
  // GL_HOOK_WRAPPER(void, glStencilFuncSeparate, (GLenum face, GLenum func, GLint ref, GLuint mask), (face, func, ref, mask), "face=0x%x, func=0x%x, ref=%d, mask=%u", face, func, ref, mask)
  // GL_HOOK_WRAPPER(void, glStencilMask, (GLuint mask), (mask), "mask=%u", mask)
  // GL_HOOK_WRAPPER(void, glStencilOpSeparate, (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass), (face, sfail, dpfail, dppass), "face=0x%x, sfail=0x%x, dpfail=0x%x, dppass=0x%x", face, sfail, dpfail, dppass)
  // GL_HOOK_WRAPPER(void, glTexImage2D, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels), (target, level, internalformat, width, height, border, format, type, pixels), "target=0x%x, level=%d, internalformat=%d, width=%d, height=%d, border=%d, format=0x%x, type=0x%x, pixels=%p", target, level, internalformat, width, height, border, format, type, pixels)
  // GL_HOOK_WRAPPER(void, glTexImage2DMultisample, (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations), (target, samples, internalformat, width, height, fixedsamplelocations), "target=0x%x, samples=%d, internalformat=0x%x, width=%d, height=%d, fixedsamplelocations=%d", target, samples, internalformat, width, height, fixedsamplelocations)
  // GL_HOOK_WRAPPER(void, glTexParameterf, (GLenum target, GLenum pname, GLfloat param), (target, pname, param), "target=0x%x, pname=0x%x, param=%.2f", target, pname, param)
  // GL_HOOK_WRAPPER(void, glTexParameteri, (GLenum target, GLenum pname, GLint param), (target, pname, param), "target=0x%x, pname=0x%x, param=%d", target, pname, param)
  // GL_HOOK_WRAPPER(void, glTexParameteriv, (GLenum target, GLenum pname, const GLint* params), (target, pname, params), "target=0x%x, pname=0x%x, params=%p", target, pname, params)
  // GL_HOOK_WRAPPER(void, glTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels), (target, level, xoffset, yoffset, width, height, format, type, pixels), "target=0x%x, level=%d, xoffset=%d, yoffset=%d, width=%d, height=%d, format=0x%x, type=0x%x, pixels=%p", target, level, xoffset, yoffset, width, height, format, type, pixels)
  // GL_HOOK_WRAPPER(void, glUniform1fv, (GLint location, GLsizei count, const GLfloat* value), (location, count, value), "location=%d, count=%d, value=%p", location, count, value)
  // GL_HOOK_WRAPPER(void, glUniform1i, (GLint location, GLint v0), (location, v0), "location=%d, v0=%d", location, v0)
  // GL_HOOK_WRAPPER(void, glUniform1iv, (GLint location, GLsizei count, const GLint* value), (location, count, value), "location=%d, count=%d, value=%p", location, count, value)
  // GL_HOOK_WRAPPER(void, glUniform1uiv, (GLint location, GLsizei count, const GLuint* value), (location, count, value), "location=%d, count=%d, value=%p", location, count, value)
  // GL_HOOK_WRAPPER(void, glUniform2fv, (GLint location, GLsizei count, const GLfloat* value), (location, count, value), "location=%d, count=%d, value=%p", location, count, value)
  // GL_HOOK_WRAPPER(void, glUniform2iv, (GLint location, GLsizei count, const GLint* value), (location, count, value), "location=%d, count=%d, value=%p", location, count, value)
  // GL_HOOK_WRAPPER(void, glUniform2uiv, (GLint location, GLsizei count, const GLuint* value), (location, count, value), "location=%d, count=%d, value=%p", location, count, value)
  // GL_HOOK_WRAPPER(void, glUniform3fv, (GLint location, GLsizei count, const GLfloat* value), (location, count, value), "location=%d, count=%d, value=%p", location, count, value)
  // GL_HOOK_WRAPPER(void, glUniform3iv, (GLint location, GLsizei count, const GLint* value), (location, count, value), "location=%d, count=%d, value=%p", location, count, value)
  // GL_HOOK_WRAPPER(void, glUniform3uiv, (GLint location, GLsizei count, const GLuint* value), (location, count, value), "location=%d, count=%d, value=%p", location, count, value)
  // GL_HOOK_WRAPPER(void, glUniform4fv, (GLint location, GLsizei count, const GLfloat* value), (location, count, value), "location=%d, count=%d, value=%p", location, count, value)
  // GL_HOOK_WRAPPER(void, glUniform4iv, (GLint location, GLsizei count, const GLint* value), (location, count, value), "location=%d, count=%d, value=%p", location, count, value)
  // GL_HOOK_WRAPPER(void, glUniform4uiv, (GLint location, GLsizei count, const GLuint* value), (location, count, value), "location=%d, count=%d, value=%p", location, count, value)
  // GL_HOOK_WRAPPER(void, glUniformMatrix3fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (location, count, transpose, value), "location=%d, count=%d, transpose=%d, value=%p", location, count, transpose, value)
  // GL_HOOK_WRAPPER(void, glUniformMatrix4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (location, count, transpose, value), "location=%d, count=%d, transpose=%d, value=%p", location, count, transpose, value)
  // GL_HOOK_WRAPPER(void, glVertexAttrib4f, (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w), (index, x, y, z, w), "index=%u, x=%.2f, y=%.2f, z=%.2f, w=%.2f", index, x, y, z, w)
  // GL_HOOK_WRAPPER(void, glVertexAttrib4fv, (GLuint index, const GLfloat* v), (index, v), "index=%u, v=%p", index, v)
  // GL_HOOK_WRAPPER(void, glVertexAttribPointer, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer), (index, size, type, normalized, stride, pointer), "index=%u, size=%d, type=0x%x, normalized=%d, stride=%d, pointer=%p", index, size, type, normalized, stride, pointer)
  // GL_HOOK_WRAPPER(void, glViewport, (GLint x, GLint y, GLsizei width, GLsizei height), (x, y, width, height), "x=%d, y=%d, width=%d, height=%d", x, y, width, height)
  // GL_HOOK_WRAPPER(void, glGenQueries, (GLsizei n, GLuint* ids), (n, ids), "n=%d, ids=%p", n, ids)
  // GL_HOOK_WRAPPER(void, glDeleteQueries, (GLsizei n, const GLuint* ids), (n, ids), "n=%d, ids=%p", n, ids)
  // GL_HOOK_WRAPPER(void, glBeginQuery, (GLenum target, GLuint id), (target, id), "target=0x%x, id=%u", target, id)
  // GL_HOOK_WRAPPER(void, glEndQuery, (GLenum target), (target), "target=0x%x", target)
  // GL_HOOK_WRAPPER(void, glGetQueryiv, (GLenum target, GLenum pname, GLint* params), (target, pname, params), "target=0x%x, pname=0x%x, params=%p", target, pname, params)
  // GL_HOOK_WRAPPER(void, glGetQueryObjectuiv, (GLuint id, GLenum pname, GLuint* params), (id, pname, params), "id=%u, pname=0x%x, params=%p", id, pname, params)
  // GL_HOOK_WRAPPER(void, glBindVertexArray, (GLuint array), (array), "array=%u", array)
  // GL_HOOK_WRAPPER(GLboolean, glIsVertexArray, (GLuint array), (array), "array=%u", array)
  // GL_HOOK_WRAPPER(void, glDeleteVertexArrays, (GLsizei n, const GLuint* arrays), (n, arrays), "n=%d, arrays=%p", n, arrays)
  // GL_HOOK_WRAPPER(void, glGenVertexArrays, (GLsizei n, GLuint* arrays), (n, arrays), "n=%d, arrays=%p", n, arrays)
  // GL_HOOK_WRAPPER(void, glTexImage3D, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels), (target, level, internalformat, width, height, depth, border, format, type, pixels), "target=0x%x, level=%d, internalformat=%d, width=%d, height=%d, depth=%d, border=%d, format=0x%x, type=0x%x, pixels=%p", target, level, internalformat, width, height, depth, border, format, type, pixels)
  // GL_HOOK_WRAPPER(void, glTexSubImage3D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels), (target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels), "target=0x%x, level=%d, xoffset=%d, yoffset=%d, zoffset=%d, width=%d, height=%d, depth=%d, format=0x%x, type=0x%x, pixels=%p", target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels)
  // GL_HOOK_WRAPPER(void, glCompressedTexSubImage3D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data), (target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data), "target=0x%x, level=%d, xoffset=%d, yoffset=%d, zoffset=%d, width=%d, height=%d, depth=%d, format=0x%x, imageSize=%d, data=%p", target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data)
  // GL_HOOK_WRAPPER(void, glCompressedTexImage3D, (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data), (target, level, internalformat, width, height, depth, border, imageSize, data), "target=0x%x, level=%d, internalformat=0x%x, width=%d, height=%d, depth=%d, border=%d, imageSize=%d, data=%p", target, level, internalformat, width, height, depth, border, imageSize, data)
  // GL_HOOK_WRAPPER(void, glTexStorage2D, (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height), (target, levels, internalformat, width, height), "target=0x%x, levels=%d, internalformat=0x%x, width=%d, height=%d", target, levels, internalformat, width, height)
  // GL_HOOK_WRAPPER(void, glTexStorage3D, (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth), (target, levels, internalformat, width, height, depth), "target=0x%x, levels=%d, internalformat=0x%x, width=%d, height=%d, depth=%d", target, levels, internalformat, width, height, depth)
  // GL_HOOK_WRAPPER(void, glBlitFramebuffer, (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter), (srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter), "srcX0=%d, srcY0=%d, srcX1=%d, srcY1=%d, dstX0=%d, dstY0=%d, dstX1=%d, dstY1=%d, mask=0x%x, filter=0x%x", srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter)
  // GL_HOOK_WRAPPER(void, glRenderbufferStorageMultisample, (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height), (target, samples, internalformat, width, height), "target=0x%x, samples=%d, internalformat=0x%x, width=%d, height=%d", target, samples, internalformat, width, height)
  // GL_HOOK_WRAPPER(void, glGetIntegeri_v, (GLenum target, GLuint index, GLint* data), (target, index, data), "target=0x%x, index=%u, data=%p", target, index, data)
  // GL_HOOK_WRAPPER(void*, glMapBufferRange, (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access), (target, offset, length, access), "target=0x%x, offset=%ld, length=%ld, access=0x%x", target, offset, length, access)
  // GL_HOOK_WRAPPER(GLboolean, glUnmapBuffer, (GLenum target), (target), "target=0x%x", target)
  // GL_HOOK_WRAPPER(void, glFlushMappedBufferRange, (GLenum target, GLintptr offset, GLsizeiptr length), (target, offset, length), "target=0x%x, offset=%ld, length=%ld", target, offset, length)
  // GL_HOOK_WRAPPER(void, glInvalidateFramebuffer, (GLenum target, GLsizei numAttachments, const GLenum* attachments), (target, numAttachments, attachments), "target=0x%x, numAttachments=%d, attachments=%p", target, numAttachments, attachments)
  // GL_HOOK_WRAPPER(void, glDrawArraysInstanced, (GLenum mode, GLint first, GLsizei count, GLsizei instancecount), (mode, first, count, instancecount), "mode=0x%x, first=%d, count=%d, instancecount=%d", mode, first, count, instancecount)
  // GL_HOOK_WRAPPER(void, glCopyBufferSubData, (GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size), (readTarget, writeTarget, readOffset, writeOffset, size), "readTarget=0x%x, writeTarget=0x%x, readOffset=%ld, writeOffset=%ld, size=%ld", readTarget, writeTarget, readOffset, writeOffset, size)
  // GL_HOOK_WRAPPER(void, glDrawBuffers, (GLsizei n, const GLenum* bufs), (n, bufs), "n=%d, bufs=%p", n, bufs)
  // GL_HOOK_WRAPPER(void, glReadBuffer, (GLenum src), (src), "src=0x%x", src)
  // GL_HOOK_WRAPPER(void, glFramebufferTextureLayer, (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer), (target, attachment, texture, level, layer), "target=0x%x, attachment=0x%x, texture=%u, level=%d, layer=%d", target, attachment, texture, level, layer)
  // GL_HOOK_WRAPPER(void, glFramebufferTexture, (GLenum target, GLenum attachment, GLuint texture, GLint level), (target, attachment, texture, level), "target=0x%x, attachment=0x%x, texture=%u, level=%d", target, attachment, texture, level)
  // GL_HOOK_WRAPPER(void, glBindBufferBase, (GLenum target, GLuint index, GLuint buffer), (target, index, buffer), "target=0x%x, index=%u, buffer=%u", target, index, buffer)
  // GL_HOOK_WRAPPER(void, glBindBufferRange, (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size), (target, index, buffer, offset, size), "target=0x%x, index=%u, buffer=%u, offset=%ld, size=%ld", target, index, buffer, offset, size)
  // GL_HOOK_WRAPPER(void, glGetActiveUniformsiv, (GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params), (program, uniformCount, uniformIndices, pname, params), "program=%u, uniformCount=%d, uniformIndices=%p, pname=0x%x, params=%p", program, uniformCount, uniformIndices, pname, params)
  // GL_HOOK_WRAPPER(GLuint, glGetUniformBlockIndex, (GLuint program, const GLchar* uniformBlockName), (program, uniformBlockName), "program=%u, uniformBlockName=%s", program, uniformBlockName)
  // GL_HOOK_WRAPPER(void, glGetUniformIndices, (GLuint program, GLsizei uniformCount, const GLchar* const* uniformNames, GLuint* uniformIndices), (program, uniformCount, uniformNames, uniformIndices), "program=%u, uniformCount=%d, uniformNames=%p, uniformIndices=%p", program, uniformCount, uniformNames, uniformIndices)
  // GL_HOOK_WRAPPER(void, glGetActiveUniformBlockiv, (GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params), (program, uniformBlockIndex, pname, params), "program=%u, uniformBlockIndex=%u, pname=0x%x, params=%p", program, uniformBlockIndex, pname, params)
  // GL_HOOK_WRAPPER(void, glGetActiveUniformBlockName, (GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName), (program, uniformBlockIndex, bufSize, length, uniformBlockName), "program=%u, uniformBlockIndex=%u, bufSize=%d, length=%p, uniformBlockName=%p", program, uniformBlockIndex, bufSize, length, uniformBlockName)
  // GL_HOOK_WRAPPER(void, glUniformBlockBinding, (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding), (program, uniformBlockIndex, uniformBlockBinding), "program=%u, uniformBlockIndex=%u, uniformBlockBinding=%u", program, uniformBlockIndex, uniformBlockBinding)
  // GL_HOOK_WRAPPER(void, glVertexAttribIPointer, (GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer), (index, size, type, stride, pointer), "index=%u, size=%d, type=0x%x, stride=%d, pointer=%p", index, size, type, stride, pointer)
  // GL_HOOK_WRAPPER(void, glGetProgramBinary, (GLuint program, GLsizei bufSize, GLsizei* length, GLenum* binaryFormat, void* binary), (program, bufSize, length, binaryFormat, binary), "program=%u, bufSize=%d, length=%p, binaryFormat=%p, binary=%p", program, bufSize, length, binaryFormat, binary)
  // GL_HOOK_WRAPPER(void, glProgramBinary, (GLuint program, GLenum binaryFormat, const void* binary, GLsizei length), (program, binaryFormat, binary, length), "program=%u, binaryFormat=0x%x, binary=%p, length=%d", program, binaryFormat, binary, length)
  // GL_HOOK_WRAPPER(void, glProgramParameteri, (GLuint program, GLenum pname, GLint value), (program, pname, value), "program=%u, pname=0x%x, value=%d", program, pname, value)
  // GL_HOOK_WRAPPER(void, glGenSamplers, (GLsizei count, GLuint* samplers), (count, samplers), "count=%d, samplers=%p", count, samplers)
  // GL_HOOK_WRAPPER(void, glDeleteSamplers, (GLsizei count, const GLuint* samplers), (count, samplers), "count=%d, samplers=%p", count, samplers)
  // GL_HOOK_WRAPPER(void, glBindSampler, (GLuint unit, GLuint sampler), (unit, sampler), "unit=%u, sampler=%u", unit, sampler)
  // GL_HOOK_WRAPPER(void, glSamplerParameteri, (GLuint sampler, GLenum pname, GLint param), (sampler, pname, param), "sampler=%u, pname=0x%x, param=%d", sampler, pname, param)
  // GL_HOOK_WRAPPER(void, glGetInternalformativ, (GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint* params), (target, internalformat, pname, bufSize, params), "target=0x%x, internalformat=0x%x, pname=0x%x, bufSize=%d, params=%p", target, internalformat, pname, bufSize, params)
  // GL_HOOK_WRAPPER(GLsync, glFenceSync, (GLenum condition, GLbitfield flags), (condition, flags), "condition=0x%x, flags=0x%x", condition, flags)
  // GL_HOOK_WRAPPER(GLenum, glClientWaitSync, (GLsync sync, GLbitfield flags, GLuint64 timeout), (sync, flags, timeout), "sync=%p, flags=0x%x, timeout=%lu", sync, flags, timeout)
  // GL_HOOK_WRAPPER(void, glDeleteSync, (GLsync sync), (sync), "sync=%p", sync)
  // GL_HOOK_WRAPPER(void, glClearBufferuiv, (GLenum buffer, GLint drawbuffer, const GLuint* value), (buffer, drawbuffer, value), "buffer=0x%x, drawbuffer=%d, value=%p", buffer, drawbuffer, value)
  // GL_HOOK_WRAPPER(void, glClearBufferfv, (GLenum buffer, GLint drawbuffer, const GLfloat* value), (buffer, drawbuffer, value), "buffer=0x%x, drawbuffer=%d, value=%p", buffer, drawbuffer, value)
  // GL_HOOK_WRAPPER(void, glClearBufferfi, (GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil), (buffer, drawbuffer, depth, stencil), "buffer=0x%x, drawbuffer=%d, depth=%.2f, stencil=%d", buffer, drawbuffer, depth, stencil)
  // GL_HOOK_WRAPPER(void, glProgramUniform1fv, (GLuint program, GLint location, GLsizei count, const GLfloat* value), (program, location, count, value), "program=%u, location=%d, count=%d, value=%p", program, location, count, value)
  // GL_HOOK_WRAPPER(void, glProgramUniform1iv, (GLuint program, GLint location, GLsizei count, const GLint* value), (program, location, count, value), "program=%u, location=%d, count=%d, value=%p", program, location, count, value)
  // GL_HOOK_WRAPPER(void, glProgramUniform2fv, (GLuint program, GLint location, GLsizei count, const GLfloat* value), (program, location, count, value), "program=%u, location=%d, count=%d, value=%p", program, location, count, value)
  // GL_HOOK_WRAPPER(void, glProgramUniform2iv, (GLuint program, GLint location, GLsizei count, const GLint* value), (program, location, count, value), "program=%u, location=%d, count=%d, value=%p", program, location, count, value)
  // GL_HOOK_WRAPPER(void, glProgramUniform3fv, (GLuint program, GLint location, GLsizei count, const GLfloat* value), (program, location, count, value), "program=%u, location=%d, count=%d, value=%p", program, location, count, value)
  // GL_HOOK_WRAPPER(void, glProgramUniform3iv, (GLuint program, GLint location, GLsizei count, const GLint* value), (program, location, count, value), "program=%u, location=%d, count=%d, value=%p", program, location, count, value)
  // GL_HOOK_WRAPPER(void, glProgramUniform4fv, (GLuint program, GLint location, GLsizei count, const GLfloat* value), (program, location, count, value), "program=%u, location=%d, count=%d, value=%p", program, location, count, value)
  // GL_HOOK_WRAPPER(void, glProgramUniform4iv, (GLuint program, GLint location, GLsizei count, const GLint* value), (program, location, count, value), "program=%u, location=%d, count=%d, value=%p", program, location, count, value)
  // GL_HOOK_WRAPPER(void, glProgramUniformMatrix2fv, (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (program, location, count, transpose, value), "program=%u, location=%d, count=%d, transpose=%d, value=%p", program, location, count, transpose, value)
  // GL_HOOK_WRAPPER(void, glProgramUniformMatrix3fv, (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (program, location, count, transpose, value), "program=%u, location=%d, count=%d, transpose=%d, value=%p", program, location, count, transpose, value)
  // GL_HOOK_WRAPPER(void, glProgramUniformMatrix4fv, (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (program, location, count, transpose, value), "program=%u, location=%d, count=%d, transpose=%d, value=%p", program, location, count, transpose, value)
  // GL_HOOK_WRAPPER(void, glProgramUniformMatrix2x3fv, (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (program, location, count, transpose, value), "program=%u, location=%d, count=%d, transpose=%d, value=%p", program, location, count, transpose, value)
  // GL_HOOK_WRAPPER(void, glProgramUniformMatrix3x2fv, (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (program, location, count, transpose, value), "program=%u, location=%d, count=%d, transpose=%d, value=%p", program, location, count, transpose, value)
  // GL_HOOK_WRAPPER(void, glProgramUniformMatrix2x4fv, (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (program, location, count, transpose, value), "program=%u, location=%d, count=%d, transpose=%d, value=%p", program, location, count, transpose, value)
  // GL_HOOK_WRAPPER(void, glProgramUniformMatrix4x2fv, (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (program, location, count, transpose, value), "program=%u, location=%d, count=%d, transpose=%d, value=%p", program, location, count, transpose, value)
  // GL_HOOK_WRAPPER(void, glProgramUniformMatrix3x4fv, (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (program, location, count, transpose, value), "program=%u, location=%d, count=%d, transpose=%d, value=%p", program, location, count, transpose, value)
  // GL_HOOK_WRAPPER(void, glProgramUniformMatrix4x3fv, (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (program, location, count, transpose, value), "program=%u, location=%d, count=%d, transpose=%d, value=%p", program, location, count, transpose, value)
  // GL_HOOK_WRAPPER(void, glProgramUniform1uiv, (GLuint program, GLint location, GLsizei count, const GLuint* value), (program, location, count, value), "program=%u, location=%d, count=%d, value=%p", program, location, count, value)
  // GL_HOOK_WRAPPER(void, glProgramUniform2uiv, (GLuint program, GLint location, GLsizei count, const GLuint* value), (program, location, count, value), "program=%u, location=%d, count=%d, value=%p", program, location, count, value)
  // GL_HOOK_WRAPPER(void, glProgramUniform3uiv, (GLuint program, GLint location, GLsizei count, const GLuint* value), (program, location, count, value), "program=%u, location=%d, count=%d, value=%p", program, location, count, value)
  // GL_HOOK_WRAPPER(void, glProgramUniform4uiv, (GLuint program, GLint location, GLsizei count, const GLuint* value), (program, location, count, value), "program=%u, location=%d, count=%d, value=%p", program, location, count, value)
  // GL_HOOK_WRAPPER(void, glBindImageTexture, (GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format), (unit, texture, level, layered, layer, access, format), "unit=%u, texture=%u, level=%d, layered=%d, layer=%d, access=0x%x, format=0x%x", unit, texture, level, layered, layer, access, format)
  // GL_HOOK_WRAPPER(void, glDispatchCompute, (GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z), (num_groups_x, num_groups_y, num_groups_z), "num_groups_x=%u, num_groups_y=%u, num_groups_z=%u", num_groups_x, num_groups_y, num_groups_z)
  // GL_HOOK_WRAPPER(void, glDispatchComputeIndirect, (GLintptr indirect), (indirect), "indirect=%ld", indirect)
  // GL_HOOK_WRAPPER(void, glGetProgramInterfaceiv, (GLuint program, GLenum programInterface, GLenum pname, GLint* params), (program, programInterface, pname, params), "program=%u, programInterface=0x%x, pname=0x%x, params=%p", program, programInterface, pname, params)
  // GL_HOOK_WRAPPER(void, glGetProgramResourceName, (GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei* length, GLchar* name), (program, programInterface, index, bufSize, length, name), "program=%u, programInterface=0x%x, index=%u, bufSize=%d, length=%p, name=%p", program, programInterface, index, bufSize, length, name)
  // GL_HOOK_WRAPPER(void, glGetProgramResourceiv, (GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum* props, GLsizei bufSize, GLsizei* length, GLint* params), (program, programInterface, index, propCount, props, bufSize, length, params), "program=%u, programInterface=0x%x, index=%u, propCount=%d, props=%p, bufSize=%d, length=%p, params=%p", program, programInterface, index, propCount, props, bufSize, length, params)
  // GL_HOOK_WRAPPER(GLuint, glGetProgramResourceIndex, (GLuint program, GLenum programInterface, const GLchar* name), (program, programInterface, name), "program=%u, programInterface=0x%x, name=%s", program, programInterface, name)
  // GL_HOOK_WRAPPER(void, glDrawArraysIndirect, (GLenum mode, const void* indirect), (mode, indirect), "mode=0x%x, indirect=%p", mode, indirect)
  // GL_HOOK_WRAPPER(void, glMemoryBarrier, (GLbitfield barriers), (barriers), "barriers=0x%x", barriers)
  // GL_HOOK_WRAPPER(void, glPatchParameteri, (GLenum pname, GLint value), (pname, value), "pname=0x%x, value=%d", pname, value)
  // GL_HOOK_WRAPPER(void, glCopyImageSubData, (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth), (srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth), "srcName=%u, srcTarget=0x%x, srcLevel=%d, srcX=%d, srcY=%d, srcZ=%d, dstName=%u, dstTarget=0x%x, dstLevel=%d, dstX=%d, dstY=%d, dstZ=%d, srcWidth=%d, srcHeight=%d, srcDepth=%d", srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth)
  // GL_HOOK_WRAPPER(void, glTexStorage3DMultisample, (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations), (target, samples, internalformat, width, height, depth, fixedsamplelocations), "target=0x%x, samples=%d, internalformat=0x%x, width=%d, height=%d, depth=%d, fixedsamplelocations=%d", target, samples, internalformat, width, height, depth, fixedsamplelocations)
  // GL_HOOK_WRAPPER(void, glDrawElementsInstancedBaseVertex, (GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount, GLint basevertex), (mode, count, type, indices, instancecount, basevertex), "mode=0x%x, count=%d, type=0x%x, indices=%p, instancecount=%d, basevertex=%d", mode, count, type, indices, instancecount, basevertex)
  // GL_HOOK_WRAPPER(void, glBlendFuncSeparatei, (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha), (buf, srcRGB, dstRGB, srcAlpha, dstAlpha), "buf=%u, srcRGB=0x%x, dstRGB=0x%x, srcAlpha=0x%x, dstAlpha=0x%x", buf, srcRGB, dstRGB, srcAlpha, dstAlpha)
  // GL_HOOK_WRAPPER(void, glBlendEquationi, (GLuint buf, GLenum mode), (buf, mode), "buf=%u, mode=0x%x", buf, mode)
  // GL_HOOK_WRAPPER(void, glBlendEquationSeparatei, (GLuint buf, GLenum modeRGB, GLenum modeAlpha), (buf, modeRGB, modeAlpha), "buf=%u, modeRGB=0x%x, modeAlpha=0x%x", buf, modeRGB, modeAlpha)
  // GL_HOOK_WRAPPER(void, glColorMaski, (GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a), (index, r, g, b, a), "index=%u, r=%d, g=%d, b=%d, a=%d", index, r, g, b, a)
  // GL_HOOK_WRAPPER(void, glGetQueryObjectui64v, (GLuint id, GLenum pname, GLuint64* params), (id, pname, params), "id=%u, pname=0x%x, params=%p", id, pname, params)
  // GL_HOOK_WRAPPER(void, glDrawBuffer, (GLenum buf), (buf), "buf=0x%x", buf)
  // GL_HOOK_WRAPPER(void, glPolygonMode, (GLenum face, GLenum mode), (face, mode), "face=0x%x, mode=0x%x", face, mode)
  // GL_HOOK_WRAPPER(void, glClearDepth, (GLdouble depth), (depth), "depth=%.2f", depth)
  // GL_HOOK_WRAPPER(void, glGetTextureParameteriv, (GLuint texture, GLenum pname, GLint* params), (texture, pname, params), "texture=%u, pname=0x%x, params=%p", texture, pname, params)
  // GL_HOOK_WRAPPER(void, glGetTextureLevelParameteriv, (GLuint texture, GLint level, GLenum pname, GLint* params), (texture, level, pname, params), "texture=%u, level=%d, pname=0x%x, params=%p", texture, level, pname, params)
  // GL_HOOK_WRAPPER(void, glRenderbufferStorageMultisampleEXT, (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height), (target, samples, internalformat, width, height), "target=0x%x, samples=%d, internalformat=0x%x, width=%d, height=%d", target, samples, internalformat, width, height)
  // GL_HOOK_WRAPPER(void, glFramebufferTexture2DMultisampleEXT, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples), (target, attachment, textarget, texture, level, samples), "target=0x%x, attachment=0x%x, textarget=0x%x, texture=%u, level=%d, samples=%d", target, attachment, textarget, texture, level, samples)

  // GL_HOOK_WRAPPER(void, glDebugMessageControl, (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled), (source, type, severity, count, ids, enabled), "source=0x%x, type=0x%x, severity=0x%x, count=%d, ids=%p, enabled=%d", source, type, severity, count, ids, enabled)
  // GL_HOOK_WRAPPER(void, glDebugMessageCallback, (GLDEBUGPROC callback, const void* userParam), (callback, userParam), "callback=%p, userParam=%p", callback, userParam)
  // GL_HOOK_WRAPPER(void, glDebugMessageInsert, (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* buf), (source, type, id, severity, length, buf), "source=0x%x, type=0x%x, id=%u, severity=0x%x, length=%d, buf=%p", source, type, id, severity, length, buf)
  // GL_HOOK_WRAPPER(void, glObjectLabel, (GLenum identifier, GLuint name, GLsizei length, const GLchar* label), (identifier, name, length, label), "identifier=0x%x, name=%u, length=%d, label=%p", identifier, name, length, label)
  // GL_HOOK_WRAPPER(void, glGetObjectLabel, (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, GLchar* label), (identifier, name, bufSize, length, label), "identifier=0x%x, name=%u, bufSize=%d, length=%p, label=%p", identifier, name, bufSize, length, label)
  // GL_HOOK_WRAPPER(void, glPushDebugGroup, (GLenum source, GLuint id, GLsizei length, const GLchar* message), (source, id, length, message), "source=0x%x, id=%u, length=%d, message=%p", source, id, length, message)
  // GL_HOOK_WRAPPER(void, glPopDebugGroup, (void), (), "")
  // GL_HOOK_WRAPPER(void, glPushGroupMarkerEXT, (GLsizei length, const GLchar* marker), (length, marker), "length=%d, marker=%p", length, marker)
  // GL_HOOK_WRAPPER(void, glPopGroupMarkerEXT, (void), (), "")
  // GL_HOOK_WRAPPER(void, glLabelObjectEXT, (GLenum type, GLuint object, GLsizei length, const GLchar* label), (type, object, length, label), "type=0x%x, object=%u, length=%d, label=%p", type, object, length, label)
  // GL_HOOK_WRAPPER(void, glGetObjectLabelEXT, (GLenum type, GLuint object, GLsizei bufSize, GLsizei* length, GLchar* label), (type, object, bufSize, length, label), "type=0x%x, object=%u, bufSize=%d, length=%p, label=%p", type, object, bufSize, length, label)
  // GL_HOOK_WRAPPER(void, glTexBuffer, (GLenum target, GLenum internalformat, GLuint buffer), (target, internalformat, buffer), "target=0x%x, internalformat=0x%x, buffer=%u", target, internalformat, buffer)
  // GL_HOOK_WRAPPER(void, glTextureView, (GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers), (texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers), "texture=%u, target=0x%x, origtexture=%u, internalformat=0x%x, minlevel=%u, numlevels=%u, minlayer=%u, numlayers=%u", texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers)
  // GL_HOOK_WRAPPER(void, glTexPageCommitmentARB, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit), (target, level, xoffset, yoffset, zoffset, width, height, depth, commit), "target=0x%x, level=%d, xoffset=%d, yoffset=%d, zoffset=%d, width=%d, height=%d, depth=%d, commit=%d", target, level, xoffset, yoffset, zoffset, width, height, depth, commit)
  // GL_HOOK_WRAPPER(void, glBlendBarrierKHR, (void), (), "")
  // GL_HOOK_WRAPPER(void, glTexStorage2DMultisample, (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations), (target, samples, internalformat, width, height, fixedsamplelocations), "target=0x%x, samples=%d, internalformat=0x%x, width=%d, height=%d, fixedsamplelocations=%d", target, samples, internalformat, width, height, fixedsamplelocations)

  void *dlsym(void *handle, const char *symbol) {
    static int in_dlsym = 0;

    if (in_dlsym) {
      return real_dlsym_bootstrap(handle, symbol);
    }

    in_dlsym = 1;

    if (!real_dlsym) {
        real_dlsym = (void *(*)(void *, const char *))real_dlsym_bootstrap(RTLD_NEXT, "dlsym");
    }

    printf_if_debug("[HOOK] dlsym called for: %s\n", symbol);

    auto iter = gl_func_registry.find(std::string(symbol));

    if (iter != gl_func_registry.end()) {

      if (*iter->second.real_ptr == nullptr) {
        *iter->second.real_ptr = real_dlsym(handle, symbol);
      }

      in_dlsym = 0;
      return iter->second.wrapper_ptr;
    }

    in_dlsym = 0;
    return real_dlsym ? real_dlsym(handle, symbol) : nullptr;
  }

  void* dlopen(const char* filename, int flags) {
    static dlopen_fptr real_dlopen = nullptr;
    if (!real_dlopen) {
      real_dlopen = reinterpret_cast<dlopen_fptr>(
        real_dlsym_bootstrap(RTLD_NEXT, "dlopen")
      );
    }
    
    printf_if_debug("[HOOK] dlopen(%s, %d)\n", filename ? filename : "NULL", flags);
    void* file_handle = real_dlopen(filename, flags);
    printf_if_debug("       -> %p\n", file_handle);
    return file_handle;
  }
} 

