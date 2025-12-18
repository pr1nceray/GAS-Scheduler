#pragma once
#include "../daemon/daemon.h"

extern interceptor_daemon::ring_buffer_shared * ring_buff;
extern std::atomic<int> * sleep_var; 
extern int atomic_var_index;
extern pid_t proc_id;
extern int shm_file_d;
extern uint64_t message_id;

void set_pid();
void open_shared_socket();
bool send_message_socket(interceptor_daemon::operation_reqs req_type, const char * message, float fps=0.0);
bool send_message_socket_if_debug(interceptor_daemon::operation_reqs req_type, const char * message, float fps=0.0);