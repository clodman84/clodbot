#include "control.h"
#include "settings.h"
#include "video.h"
#include <raylib.h>
#include <stdio.h>
#include <vlc/vlc.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

static void draw_settings() {
  int PANEL_X = 24;
  int PANEL_Y = 24;
  int PANEL_WIDTH = 300;
  int PANEL_HEIGHT = 352;
  int SLIDER_X = PANEL_X + 76;
  int SLIDER_Y = PANEL_Y + 38;
  int SLIDER_WIDTH = 212;
  int SLIDER_HEIGHT = 22;
  int NEXT = 28;

  int WIFI_X = PANEL_X + 12;
  int WIFI_WIDTH = SLIDER_X + SLIDER_WIDTH - WIFI_X;
  int WIFI_HEIGHT = SLIDER_HEIGHT;
  int WIFI_Y = SLIDER_Y + NEXT * 3.2;

  GuiPanel((Rectangle){PANEL_X, PANEL_Y, PANEL_WIDTH, PANEL_HEIGHT},
           "#141#Settings");
  GuiSliderBar((Rectangle){SLIDER_X, SLIDER_Y, SLIDER_WIDTH, SLIDER_HEIGHT},
               "Brightness", "", &settings.brightness, 0, 100);
  GuiSliderBar(
      (Rectangle){SLIDER_X, SLIDER_Y + NEXT, SLIDER_WIDTH, SLIDER_HEIGHT},
      "Volume    ", "", &settings.volume, 0, 100);

  char wifi_state_string[64];

  switch (wifi.state) {
  case NM_STATE_CONNECTED_SITE:
  case NM_STATE_CONNECTED_LOCAL:
    sprintf(wifi_state_string, "#189# Connected (No Internet)");
    break;
  case NM_STATE_CONNECTED_GLOBAL:
    GBytes *ssid_bytes = nm_access_point_get_ssid(wifi.current_ap);
    gsize len;
    const char *data = g_bytes_get_data(ssid_bytes, &len);
    char *ssid = g_strndup(data, len);
    nm_access_point_get_strength(wifi.current_ap);
    sprintf(wifi_state_string, "#189# Connected to %s", ssid);
    break;
  case NM_STATE_CONNECTING:
    sprintf(wifi_state_string, "#189# Connecting...");
    break;
  case NM_STATE_DISCONNECTING:
    sprintf(wifi_state_string, "#189# Disconnecting...");
    break;
  case NM_STATE_DISCONNECTED:
    sprintf(wifi_state_string, "#189# Disconnected");
    break;
  case NM_STATE_ASLEEP:
    sprintf(wifi_state_string, "#189# Asleep");
    break;
  case NM_STATE_UNKNOWN:
    sprintf(wifi_state_string, "#189# Unknown");
    break;
  }
  GuiLine((Rectangle){PANEL_X, SLIDER_Y + NEXT * 2.5, PANEL_WIDTH, 0},
          wifi_state_string);

  if (GuiButton((Rectangle){WIFI_X, WIFI_Y, 80, 20}, "Scan")) {
    wifi_scan();
  }

  int y = 1;
  GHashTableIter iter;
  gpointer key, value;
  char label[128];
  g_hash_table_iter_init(&iter, wifi.best_aps);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    NMAccessPoint *ap = value;
    snprintf(label, sizeof(label), "%-15s (%u%%)", (char *)key,
             nm_access_point_get_strength(ap));
    GuiButton((Rectangle){WIFI_X, WIFI_Y + NEXT * y, WIFI_WIDTH, WIFI_HEIGHT},
              label);
    y += 1;
  }
}

int main() {
  const unsigned int frame_width = 800, frame_height = 480;
  libvlc_instance_t *vlc_instance;
  char const *vlc_argv[] = {"--no-audio", "--no-xlib"};

  // set up vlc
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

  // set up control socket
  Remote *remote = start_listening();

  // set up wifi
  wifi_init();

  SetTargetFPS(60);
  InitWindow(frame_width, frame_height, "clodbot");
  GuiLoadStyle("./style_amber.rgs");
  GuiSetAlpha(0.9);
  Image image = {.data = video->buffer,
                 .width = frame_width,
                 .height = frame_height,
                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
                 .mipmaps = 1};

  video->texture = LoadTextureFromImage(image);
  libvlc_media_player_play(video->player);

  float position = 0.0;
  bool show_settings = true;

  while (!WindowShouldClose()) {
    g_main_context_iteration(NULL, FALSE);
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
        show_settings = !show_settings;
    }
    pthread_mutex_unlock(&remote->state_mutex);

    position = libvlc_media_player_get_position(video->player);
    if (position > 0.99) // floating point precision shenanigans
      libvlc_media_player_set_position(video->player, 0.0);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(video->texture, 0, 0, WHITE);

    if (GuiButton((Rectangle){676, 24, 100, 24}, "#191#Settings"))
      show_settings = !show_settings;

    if (show_settings)
      draw_settings();

    EndDrawing();
  }
  UnloadTexture(video->texture);
  free(video->buffer);
  libvlc_release(vlc_instance);

  return 0;
}
