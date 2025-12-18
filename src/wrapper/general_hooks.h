#pragma once
#include <cstddef>  
#include <cstdint>   
#include <cstdio>    
#include <string_view> 
#include <dlfcn.h>   
#include <unistd.h>
#include <thread>
#include <cstring>
#include <iostream>
#include <chrono>
#include <cstdarg>
#include <pthread.h>
#include <condition_variable>



#include "debug_printf.h"
#include "common_types.h"
#include "daemon_comm.h"
#include "plthook.h"


using dlopen_fptr = void* (*)(const char*, int);
using dlsym_fptr = void *(*)(void *, const char *);

enum class job_type {
  OpenGL,
  SDL2,
  Vulkan
};



extern void *(*real_dlsym)(void *, const char *);


void * real_dlsym_bootstrap(void* handle, const char* symbol);

