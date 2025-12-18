#pragma once 
#include "daemon_types.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sstream>
#include <memory>
#include <nvml.h>
// initialization message format :

// pid_t process_id


// request gpu scheduling message format

// pid_t process_id
// uint8_t func_name_len
// func_name_len bytes to name the function being called

// todo : maybe have a seperate thread which just reads gpu information
// to get things like 

namespace interceptor_daemon
{

  constexpr std::string_view socket_loc = "/gpu-scheduler";
  constexpr std::string_view client_socket_loc = "/tmp/gpu-scheduler_";

  extern std::unordered_map<pid_t, process_information> process_status;
  extern std::unordered_map<int, int> pid_to_index_map;
  extern std::unordered_map<int, int> index_to_pid_map;

  extern std::deque<int> available_index;
  
  //ddint have time to make a hazard pointer so 
  // this will have to work.
  extern std::atomic<std::shared_ptr<gpu_information_wrapper>> proc_gpu_info;  
  
  void update_gpu_info();

  void setup_mappings();
  void setup_nmvl(unsigned int device_index);

  std::string get_socket_location(pid_t proc_id);

  void parse_buffer(char * buff, shared_wrapper * shared_mem);


}
