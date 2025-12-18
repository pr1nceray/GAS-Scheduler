#pragma once 
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <string_view>
#include <string>
#include <cstring>
#include <cmath>
#include <stdlib.h>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <deque>
#include <immintrin.h>
#include "../wrapper/debug_printf.h"

namespace interceptor_daemon
{
  // constants
  // size being low causes fps nukes.
  constexpr int64_t NUM_SLOTS = 4096 * 4096;
  constexpr int64_t slot_len = 64;
  constexpr int64_t NUM_MICROSECONDS_IN_SEC = 1000000;
  constexpr unsigned int num_procs_using_gpu = 32;
  constexpr float epsil = 0.5;
  constexpr float epsil_usage= .05;

  enum operation_reqs : uint8_t {
    REGISTER = 0,
    REQUEST = 1,
    FRAMERATE_PING = 2,
    DEBUG_PRINT = 3,
    EXIT = 4,
    NONE = 5,
  };

  static_assert(sizeof(operation_reqs) == 1, "operation_reqs must be 1 byte");

  // struct && class defs
  struct gpu_information {
    float usage_percentage;
    int pid;
    bool active;
  };

  struct gpu_information_wrapper {
    float sum_tracked_procs;
    float sum_all_procs;
    std::chrono::steady_clock::time_point last_collected_gpu_time;
    gpu_information proc_info[num_procs_using_gpu];
    gpu_information_wrapper() {
      sum_tracked_procs = 0;
      sum_all_procs = 0;
      for (unsigned int i = 0; i < num_procs_using_gpu; ++i) {
        proc_info[i].active = false;
      }
    
    }
  };

  struct timing_information_wrapper {
    std::atomic<bool> valid_framerate_values[num_procs_using_gpu];
    std::atomic<float> framerate_values[num_procs_using_gpu];
    std::atomic<int> sleep_timings[num_procs_using_gpu];

    timing_information_wrapper() {
      for (size_t idx = 0; idx < num_procs_using_gpu; ++idx) {
        valid_framerate_values[idx].store(false);
        sleep_timings[idx].store(0);
        
      }
    }

    void clear_item (size_t idx) {
      if (idx >= num_procs_using_gpu) {
        printf_if_debug("[DAEMON] bad index request to clear_item(), got %d\n", idx);
        exit(1);
      }

      valid_framerate_values[idx].store(false, std::memory_order_release);

    }

    void set_fps_value(int idx, float fps) {
      framerate_values[idx].store(fps, std::memory_order_relaxed);
      valid_framerate_values[idx].store(true, std::memory_order_release);
    }
    
    void set_sleep_timing(int idx, int timing_mircos) {
      sleep_timings[idx].store(timing_mircos, std::memory_order_release);
    }
    
    int get_sleep_timing(int idx) {
      return sleep_timings[idx].load(std::memory_order_acquire);
    }

    float get_fps_value(int idx) {
      if (!valid_framerate_values[idx].load(std::memory_order_acquire)) {
        return NAN;
      }
      return framerate_values[idx].load(std::memory_order_acquire);
    }
  };

  class process_information {
    pid_t proc_id = -1;
    int fps = -1;
    std::chrono::steady_clock::time_point last_gpu_call_time {}; 

    public:
    int index_into_atom_list = 0;
    process_information () = delete;
    
    process_information(pid_t proc_id_in, int index_into_atom_list_in) :
      proc_id(proc_id_in),
      fps(1), 
      index_into_atom_list(index_into_atom_list_in) {
        
    }
  };



  class ring_buffer_shared {
    std::atomic<int64_t> head;
    std::atomic<int64_t> tail;

    std::atomic<bool> is_ready[NUM_SLOTS];
    char data[NUM_SLOTS][slot_len];

    public:

    std::atomic<bool> ready;

    void init_ring_buffer() {
      head.store(0,  std::memory_order_seq_cst);
      tail.store(0,  std::memory_order_seq_cst);
      for(size_t i = 0; i < NUM_SLOTS; ++i) {
        data[i][slot_len - 1] = '\0';
        is_ready[i].store(false, std::memory_order_seq_cst);
      }
      ready.store(true, std::memory_order_seq_cst);
    }

    bool push(operation_reqs req_type, int proc_id, float fps, uint64_t id, const char * message) {
      int64_t head_v = head.load(std::memory_order_relaxed);
      
      while(true) {
        int64_t tail_v = tail.load(std::memory_order_acquire);
        
        // buffer full?
        if (head_v - tail_v >= NUM_SLOTS) {
            return false;
        } 

        //ATOMICALLY check if head = head_v.
        // if so, head = head + 1. 
        // (all happens attomically). returns false if head != head_v 
        if(
          head.compare_exchange_weak(
            head_v, 
            head_v + 1, 
          std::memory_order_acq_rel, 
          std::memory_order_relaxed)
        ) {

          uint64_t idx = (head_v)%NUM_SLOTS;

          size_t total_size = 0;
          memcpy(data[idx] + total_size, &req_type, sizeof(operation_reqs));
          total_size += sizeof(operation_reqs);
          memcpy(data[idx] + total_size, &proc_id, sizeof(int));
          total_size += sizeof(int);
          memcpy(data[idx] + total_size, &fps, sizeof(float));
          total_size += sizeof(float);
          memcpy(data[idx] + total_size, &id, sizeof(uint64_t));
          total_size += sizeof(uint64_t);
          strncpy(data[idx] + total_size, message, slot_len - (1 + total_size));

          is_ready[idx].store(true, std::memory_order_release);
          return true;
        }
      }
    }

    bool pop_if_possible(char * buff) {
      uint64_t tail_v = tail.load(std::memory_order_relaxed);
      size_t idx = tail_v % NUM_SLOTS;

      if(!is_ready[idx].load(std::memory_order_acquire)) {
        return false;
      } 

      memcpy(buff, data[idx], slot_len - 1);
  
      is_ready[idx].store(false, std::memory_order_release);
      tail.fetch_add(1, std::memory_order_release);
      
      return true;
    }


  };


  struct shared_wrapper {
    ring_buffer_shared ring_buff;
    std::atomic<int> proc_sleep_vars[num_procs_using_gpu]; 
  };

}
