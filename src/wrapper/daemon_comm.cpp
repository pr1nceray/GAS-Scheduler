#include "daemon_comm.h"

int shm_file_d = 0;
int event_file_d = -1;
int atomic_var_index = -1;
uint64_t message_id = 0;

interceptor_daemon::ring_buffer_shared * ring_buff = nullptr;
std::atomic<int> * sleep_var = nullptr;

pid_t proc_id = -1;

int proc_id_to_send = -1;

// i dont know where else to put getpid()
// so i just put here
void set_pid() {
  proc_id = getpid();
  proc_id_to_send = proc_id;
}

void obtain_index() {
  const std::string path = interceptor_daemon::get_socket_location(proc_id);
  
  unlink(path.c_str());
  int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    printf_if_debug("[HOOK] failed to create per process socket, errno %d\n", socket_fd);
  }
  
  sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, path.c_str());
  int bind_res = bind(socket_fd, (sockaddr*)&addr, sizeof(addr));
  if (bind_res != 0) {
    printf_if_debug("[HOOK] binding to per proc socket FAILED: %d\n", bind_res);
    exit(1);
  }

  int listen_res = listen(socket_fd, 1);
  if (listen_res != 0) {
    printf_if_debug("[HOOK] listening to per proc socket FAILED: %d\n", listen_res);
    exit(1);
  }

  send_message_socket(interceptor_daemon::REGISTER, "\x01");
  printf_if_debug("[HOOK] sent register message\n");
  int daemon_fd = accept(socket_fd, nullptr, nullptr);
  recv(daemon_fd, &atomic_var_index, sizeof(atomic_var_index), 0);

  close(daemon_fd);
  close(socket_fd);
  unlink(path.c_str());


  printf_if_debug("[HOOK] recieved index %d from daemon\n", atomic_var_index);

}

void open_shared_socket() {
  shm_file_d = shm_open(interceptor_daemon::socket_loc.data(), O_RDWR, 0);
  if(shm_file_d < 0) {
    printf_if_debug("[HOOK] failed to open shared socket. Is daemon running?\n"); 
    exit(1); 
  }

  interceptor_daemon::shared_wrapper * shared_mem = static_cast<interceptor_daemon::shared_wrapper *>(
    mmap(
      nullptr, 
      sizeof(interceptor_daemon::shared_wrapper),
      PROT_READ | PROT_WRITE, MAP_SHARED, 
      shm_file_d, 
      0
    )
  );

  ring_buff = &(shared_mem->ring_buff);

  obtain_index();
  proc_id_to_send = atomic_var_index;
  sleep_var = &(shared_mem->proc_sleep_vars[atomic_var_index]); 



  // create socket and recieve index value from server
  
  if (!ring_buff) {
    printf_if_debug("[HOOK] failed to access shmem. Is daemon running?\n"); 
    exit(1);
  }

  return;
}

bool send_message_socket(interceptor_daemon::operation_reqs req_type, const char * message, float fps) {
  if(!ring_buff->ready.load(std::memory_order_acquire)) { 
    printf_if_debug("[HOOK] failed to send message. Is daemon ready?\n"); 
    exit(1);
  }
  while (!ring_buff->push(req_type, proc_id_to_send, fps, message_id, message)) {
    printf_if_debug("[HOOK] ringbuffer full\n");
  }
  message_id += 1;
  return true;

}

bool send_message_socket_if_debug(interceptor_daemon::operation_reqs req_type, const char * message, float fps) {
  #ifdef SOCKET_DEBUG
  if(!ring_buff->ready.load(std::memory_order_acquire)) { 
    printf_if_debug("[HOOK] failed to send message. Is daemon ready?\n"); 
    exit(1);
  }
  return ring_buff->push(req_type, proc_id_to_send, fps, message);
  #else
    return true;
  #endif

}
