#include "control.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

void *control_socket_thread(void *arg) {
  int server_fd, client_fd;
  struct sockaddr_un addr;
  unlink(CONTROL_SOCKET_PATH);

  Remote *remote = (Remote *)arg;

  server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("Socket Error");
    return NULL;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, CONTROL_SOCKET_PATH, sizeof(addr.sun_path) - 1);

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr))) {
    perror("Bind Error");
    close(server_fd);
    return NULL;
  }

  if (listen(server_fd, 5) < 0) {
    perror("Listen Error");
    close(server_fd);
    return NULL;
  }

  bool connected = false;
  while (true) {
    if (!connected) {
      client_fd = accept(server_fd, NULL, NULL);
      if (client_fd < 0) {
        perror("Accept Error");
      }
      connected = true;
      printf("Accepted Connection!\n");
    }
    ssize_t n =
        recv(client_fd, remote->buf, sizeof(remote->buf) - 1, MSG_WAITFORONE);
    if (n > 0) {
      remote->buf[n] = '\0';
      remote->has_message = true;
      printf("Received Socket Message: %s\n", remote->buf);
      send(client_fd, "Acknowledged!", 13, MSG_DONTWAIT);
    }

    if (n == 0) {
      connected = false;
      printf("Client Disconnected!\n");
      close(client_fd);
    }
  }
  return NULL;
}

Remote *start_listening() {
  Remote *remote = malloc(sizeof(Remote));
  remote->has_message = false;
  pthread_mutex_init(&remote->state_mutex, NULL);
  pthread_create(&remote->control_thread, NULL, &control_socket_thread, remote);
  return remote;
}
