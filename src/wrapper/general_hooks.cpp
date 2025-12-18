#include "general_hooks.h"


static void* (*real_dlsym_next)(void*, const char*) = nullptr;
static void* (*real_dlsym_next_sdl)(void*, const char*) = nullptr;
void *(*real_dlsym)(void *, const char *) = nullptr;


void death_message(int) {
  printf_if_debug("[HOOK] LD_PRELOAD hook finished\n");
  printf_if_debug("[HOOK] sending death message\n");
  send_message_socket(interceptor_daemon::operation_reqs::EXIT, "lol i died");
}


void * real_dlsym_bootstrap(void* handle, const char* symbol) {

  if (real_dlsym_next) {
    return real_dlsym_next(handle, symbol);
  } 

  real_dlsym_next = reinterpret_cast<void* (*)(void*, const char*)>(
    dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5")
  );

  if (!real_dlsym_next) {
    void* libc = dlopen("libc.so.6", RTLD_LAZY);
    if (libc) {
      real_dlsym_next = reinterpret_cast<void* (*)(void*, const char*)>(
        dlvsym(libc, "dlsym", "GLIBC_2.2.5")
      );
    }
  }
  return real_dlsym_next ? real_dlsym_next(handle, symbol) : nullptr;
}


void * real_dlsym_bootstrap_sdl(void* handle, const char* symbol) {

  if (real_dlsym_next_sdl) return real_dlsym_next_sdl(handle, symbol);

  real_dlsym_next_sdl = reinterpret_cast<void* (*)(void*, const char*)>(
    dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5")
  );

  if (!real_dlsym_next_sdl) {
    void* libc = dlopen("libc.so.6", RTLD_LAZY);
    if (libc) {
      real_dlsym_next_sdl = reinterpret_cast<void* (*)(void*, const char*)>(
        dlvsym(libc, "dlsym", "GLIBC_2.2.5")
      );
    }
  }
  return real_dlsym_next_sdl ? real_dlsym_next_sdl(handle, symbol) : nullptr;
}

static void __attribute__((constructor)) hook_init(void) {
  printf_if_debug("[HOOK] LD_PRELOAD hook init\n");
  printf_if_debug("[HOOK] PID: %d\n", getpid());
  set_pid();
  
  std::signal(SIGINT,  death_message);  
  std::signal(SIGTERM, death_message);

  open_shared_socket(); 
  //printf_if_debug("[HOOK] opened shared socket at %d\n", shm_file_d);
  if (!real_dlsym) {
    real_dlsym = (dlsym_fptr)real_dlsym_bootstrap(RTLD_NEXT, "dlsym");
  }
  if (!real_dlsym) {
    printf_if_debug("[HOOK] Failed to find real dlsym in initialization; exiting\n");
    exit(1);
  }

  const std::string proc_str = std::to_string(proc_id) + " connected to daemon!";
  bool res = send_message_socket_if_debug(interceptor_daemon::operation_reqs::DEBUG_PRINT, proc_str.c_str());
  if (!res) {
    printf_if_debug("[HOOK] failed to setup initial communication with daemon\n");
    exit(1);
  }

}

static void __attribute__((destructor)) hook_fini(void) {
  death_message(0);
}
