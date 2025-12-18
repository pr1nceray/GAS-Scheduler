#include "daemon_utils.h"

namespace interceptor_daemon 
{
  std::unordered_map<pid_t, process_information> process_status;
  std::unordered_map<int, int> pid_to_index_map;
  std::unordered_map<int, int> index_to_pid_map;

  std::deque<int> available_index;
  
  //ddint have time to make a hazard pointer so 
  // this will have to work.
  std::atomic<std::shared_ptr<gpu_information_wrapper>> proc_gpu_info;  
  timing_information_wrapper timing_info;
  
  nvmlDevice_t device;
  nvmlUtilization_t utilization_rates;

  void setup_mappings() {
    for (size_t idx = 0; idx < num_procs_using_gpu; ++idx) {
      available_index.push_back(idx);
    }
  }
  


  void setup_nmvl(unsigned int device_index) {
    if (nvmlInit() != NVML_SUCCESS) {
      printf_if_debug("[DAEMON] Failed to initialize NVML, exiting \n");
      exit(1);
    }

    unsigned int num_devices = -1;
    nvmlDeviceGetCount(&num_devices);
    if (device_index >= num_devices) {
      printf_if_debug("[DAEMON] attempted to access nonexistant device. exiting...\n");
      exit(1);
    }
    nvmlDeviceGetHandleByIndex(device_index, &device);
    
  }

  std::shared_ptr<gpu_information_wrapper> get_gpu_processes() {
    std::shared_ptr<gpu_information_wrapper> cur_info = std::make_shared<gpu_information_wrapper>();
    nvmlDeviceGetUtilizationRates(device, &utilization_rates);
    cur_info->sum_all_procs = utilization_rates.gpu; 

    unsigned int process_count = 32;
    std::vector<nvmlProcessUtilizationSample_t> samples(process_count);
    unsigned long long timestamp = 0;

    nvmlReturn_t result = nvmlDeviceGetProcessUtilization(device, samples.data(), &process_count, timestamp);
    
    if (result == NVML_ERROR_INSUFFICIENT_SIZE) {
      samples.resize(process_count);
      result = nvmlDeviceGetProcessUtilization(device, samples.data(), &process_count, timestamp);
    }

    if (result != NVML_SUCCESS) {
      printf_if_debug("[DAEMON] Failed to get process utilization: %s\n", nvmlErrorString(result));
      return cur_info;
    }

    cur_info->sum_tracked_procs = 0; 
    for (unsigned int i = 0; i < process_count; ++i) {
      pid_t pid = samples[i].pid;
      unsigned int sm_util = samples[i].smUtil; 
      if (process_status.count(pid)) {
        int relative_index = pid_to_index_map[pid];
          
        cur_info->proc_info[relative_index].active = true;
        cur_info->proc_info[relative_index].pid = pid;
        cur_info->proc_info[relative_index].usage_percentage = (float)sm_util / 100.0f;
        cur_info->sum_tracked_procs += cur_info->proc_info[relative_index].usage_percentage;
        printf_if_debug("[DAEMON] process with pid %d, index %d had usage %.3f\n", pid, relative_index, cur_info->proc_info[relative_index].usage_percentage);
      }
    }
    return cur_info;
  }
  
  void calculate_timings(std::shared_ptr<gpu_information_wrapper> & cur_info) {
    if (cur_info->sum_tracked_procs <  epsil_usage) {
      printf_if_debug("[DAEMON] returning early in timings due to %.3f being less than %.3f\n", cur_info->sum_tracked_procs, epsil_usage);
      return;
    }

    int count = 0;
    for (size_t i = 0; i < num_procs_using_gpu; ++i) {
      if (cur_info->proc_info[i].active) {
        count += 1;
        cur_info->proc_info[i].usage_percentage /= cur_info->sum_tracked_procs;
      }
    }
    
    if (count == 0) {
      printf_if_debug("[DAEMON] early return due to 0 count\n");
      return;
    }

    float target_usage = (cur_info->sum_tracked_procs/count)/cur_info->sum_tracked_procs;

    for (size_t i = 0; i < num_procs_using_gpu; ++i) {
      if (cur_info->proc_info[i].active) {
        // < 1 = using more gpu than fair
        // > 1 = using less gpu than fair
        
        float ratio = (target_usage /cur_info->proc_info[i].usage_percentage);
        
        if (ratio > 1) {
          printf_if_debug("[DAEMON] Skippng %zu due to high ratio of %.3f \n", i, ratio);
          // divide current sleep timing by some value?
          // update sleep timings to be 0?.
          continue;
        }

        float fps_old = timing_info.get_fps_value(i);
        if (fps_old < epsil) {
          timing_info.set_sleep_timing(i, 0);
        }
        float fps_target = fps_old * ratio;
        float microseconds_per_target = NUM_MICROSECONDS_IN_SEC/fps_target;
        float mircoseconds_per_old = NUM_MICROSECONDS_IN_SEC/fps_old;
        int num_microsends_stall = microseconds_per_target - mircoseconds_per_old;
        timing_info.set_sleep_timing(i, num_microsends_stall);
        printf_if_debug("[DAEMON] setting sleep timer for %zu to %d milliseconds \n", i, num_microsends_stall/1000);
      }
    }



  }

  void update_gpu_info () {

    std::shared_ptr<gpu_information_wrapper> cur_info = get_gpu_processes();
    cur_info->last_collected_gpu_time = std::chrono::steady_clock::now();
    calculate_timings(cur_info);
    proc_gpu_info.store(cur_info, std::memory_order_seq_cst);
    printf_if_debug("[DAEMON] updated gpu utilization, overall usage = %.3f and our usage = %.3f\n", cur_info->sum_all_procs, cur_info->sum_tracked_procs);
    
  }

  std::string get_socket_location(pid_t proc_id) {
    return std::string(client_socket_loc) + "_" + std::to_string(proc_id) + ".sock";
  }
  
  int assign_index(pid_t proc_id) {
    if (available_index.empty()) {
      printf_if_debug("[DAEMON] ran out of available information tracking slots; check cleanup or increase limit\n");
    }

    int new_index = available_index.back();
    available_index.pop_back();

    const std::string path = get_socket_location(proc_id);

    int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path.c_str());
      
    connect(socket_fd, (sockaddr*)&addr, sizeof(addr));
      
    send(socket_fd, &new_index, sizeof(new_index), 0);
    printf_if_debug("[DAEMON] assigned index %d to proc %d\n", new_index, proc_id);
    close(socket_fd);
    return new_index;
  }


  void parse_buffer(char buff[], shared_wrapper * shared_mem) {
    operation_reqs req_type = operation_reqs::NONE;
    int proc_id = -1; // proc_id is pid_t when req_type = REGISTER, index into atom list all other times
    size_t total_size = 0;
    float fps;
    uint64_t message_id;

    memcpy(&req_type, buff + total_size, sizeof(operation_reqs));
    total_size += sizeof(operation_reqs);
    memcpy(&proc_id, buff + total_size, sizeof(int));
    total_size += sizeof(int);
    memcpy(&fps, buff + total_size, sizeof(float));
    total_size += sizeof(float);
      
    memcpy(&message_id, buff + total_size, sizeof(message_id));
    total_size += sizeof(message_id);


    //printf_if_debug("[DAEMON] recieved request type %d for proc id %d. message = %s, id = %lu \n", req_type, proc_id, buff + total_size, message_id);
    switch (req_type) {
      case (REGISTER) : {
        int new_index = assign_index(proc_id);
        
        pid_to_index_map[proc_id] = new_index;
        index_to_pid_map[new_index] = proc_id;

        process_status.emplace(proc_id, process_information(proc_id, new_index));
        break;
      }
      case (REQUEST) : {
        std::shared_ptr<gpu_information_wrapper> info = proc_gpu_info.load();
        if (info->sum_all_procs < 95 || (available_index.size() == num_procs_using_gpu - 1)) {
          shared_mem->proc_sleep_vars[proc_id].store(0, std::memory_order_release);
        }
        
        shared_mem->proc_sleep_vars[proc_id].store(timing_info.sleep_timings[proc_id], std::memory_order_release);
        break;
      }
      case (DEBUG_PRINT) : {
        printf_if_debug(
          "[DAEMON] %s\n",
          buff + total_size
        );
        break;
      } 
      case (FRAMERATE_PING) : {
        float incoming_fps;
        memcpy(&incoming_fps, buff + sizeof(operation_reqs) + sizeof(int), sizeof(float));
        timing_info.set_fps_value(proc_id, incoming_fps);
        printf_if_debug("[DAEMON] recieved fps of %.3f\n", incoming_fps);
        break;
      }
      case (EXIT) : {
        available_index.push_back(proc_id);
        int actual_pid = index_to_pid_map[proc_id];
        
        pid_to_index_map.erase(actual_pid);
        index_to_pid_map.erase(proc_id);

        break;
      }
      default : {
        printf_if_debug("[DAEMON] Received bad message, dying!\n");
        exit(1);
        break;
      }
    }
  }

}
