#include "control.h"
#include "video.h"
#include <raylib.h>
#include <vlc/vlc.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

int main() {

  const unsigned int frame_width = 800, frame_height = 480;
  libvlc_instance_t *vlc_instance;

  char const *vlc_argv[] = {"--no-audio", "--no-xlib"};

  char *path = "./no_distort_264.mp4\0";
  int vlc_argc = sizeof(vlc_argv) / sizeof(*vlc_argv);
  vlc_instance = libvlc_new(vlc_argc, vlc_argv);
  if (vlc_instance == NULL) {
    printf("VLC returned a null pointer when trying to set up instance\n");
    return 1;
  } else
    printf("Successfully initialised libVLC instance\n");

  Video *video =
      make_video_from_path(vlc_instance, path, frame_width, frame_height);

  Remote *remote = start_listening();

  SetTargetFPS(60);
  InitWindow(frame_width, frame_height, "clodbot");
  GuiLoadStyle("./style_amber.rgs");
  Image image = {.data = video->buffer,
                 .width = frame_width,
                 .height = frame_height,
                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
                 .mipmaps = 1};

  video->texture = LoadTextureFromImage(image);
  libvlc_media_player_play(video->player);

  float position = 0.0;
  bool showVideo = true;

  while (!WindowShouldClose()) {
    // updating the texture if we have new data from VLC
    // maintains original FPS of the video
    if (video->needUpdate) {
      pthread_mutex_lock(&video->mutex);
      UpdateTexture(video->texture, video->buffer);
      pthread_mutex_unlock(&video->mutex);
    }

    // see whether the remote connection wants us to display the video
    pthread_mutex_lock(&remote->state_mutex);
    if (remote->has_message) {
      remote->has_message = false;
      if (strncmp(remote->buf, "toggle", 6) == 0)
        showVideo = !showVideo;
    }
    pthread_mutex_unlock(&remote->state_mutex);

    position = libvlc_media_player_get_position(video->player);
    if (position > 0.99) // floating point precision shenanigans
      libvlc_media_player_set_position(video->player, 0.0);

    BeginDrawing();
    ClearBackground(BLACK);
    if (showVideo) {
      DrawTexturePro(video->texture,
                     (Rectangle){0, 0, frame_width, frame_height},
                     (Rectangle){0, 0, frame_width, frame_height},
                     (Vector2){0, 0}, 0.0f, WHITE);
    }
    if (GuiButton((Rectangle){24, 24, 120, 30}, "#191#Toggle Video")) {
      showVideo = !showVideo;
    }
    EndDrawing();
  }

  UnloadTexture(video->texture);
  free(video->buffer);
  libvlc_release(vlc_instance);

  return 0;
}
