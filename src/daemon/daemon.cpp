
#include "./daemon.h"


interceptor_daemon::ring_buffer_shared * ring_buff = nullptr;

void handle_shutdown(int) {
  ring_buff->ready.store(false, std::memory_order_seq_cst);
}

namespace interceptor_daemon
{
int daemon_main() {
  
  setup_mappings(); 
  update_gpu_info();
  std::string xyz = std::string(socket_loc.data());
  int shm_file_d = shm_open(
    socket_loc.data(), 
    O_CREAT | O_RDWR, 0666);
    ftruncate(shm_file_d, 
    sizeof(shared_wrapper)
  );

  shared_wrapper * shared_mem = static_cast<shared_wrapper *>(
    mmap(
      nullptr, 
      sizeof(shared_wrapper),
      PROT_READ | PROT_WRITE, MAP_SHARED, 
      shm_file_d, 
      0
    )
  );

  ring_buff = &(shared_mem->ring_buff);
  
  std::signal(SIGINT,  handle_shutdown);  
  std::signal(SIGTERM, handle_shutdown);  

  ring_buff->init_ring_buffer();

  printf_if_debug("[DAEMON] ready\n");

  char buff[slot_len];

  std::thread gpu_thread(
    [&]() {
      setup_nmvl(0);
      while (true) {
        update_gpu_info();
        sleep(1);
      }
    }
  );

  std::chrono::steady_clock::time_point last_time_taken{};
  while (true) {
    if (std::chrono::steady_clock::now() - last_time_taken > std::chrono::seconds(1)) {  
      last_time_taken = std::chrono::steady_clock::now();
    }
    
    


    if (!ring_buff->ready.load(std::memory_order_acquire)) {
      exit(1);
    }

    if (ring_buff->pop_if_possible(buff)) {

      // if (!res) {
      //   printf_if_debug("[DAEMON] unable to pop from daemon despite size being > 1; exiting\n");
      //   exit(1);
      // }
      
      parse_buffer(buff, shared_mem);
      // int rbsize = ring_buff->size.load(std::memory_order_seq_cst);
      // if ((NUM_SLOTS - rbsize) < 100) {
      //   printf_if_debug("TOO MUCH CONTENTION\n");
      // }
    }
  }
}

} // namespace interceptor_daemon

int main() {
  interceptor_daemon::daemon_main();
}
