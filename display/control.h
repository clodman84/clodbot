#ifndef CONTROL_H
#define CONTROL_H

#include <pthread.h>
#include <stdbool.h>

#define CONTROL_SOCKET_PATH "/tmp/display_ctrl.sock"

typedef struct {
  bool has_message;
  char buf[128];
  pthread_mutex_t state_mutex;
  pthread_t control_thread;
} Remote;

Remote *start_listening();

#endif // !CONTROL_H
