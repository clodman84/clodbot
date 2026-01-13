#include "control.h"
#include "video.h"
#include <glib.h>
#include <libnm/NetworkManager.h>
#include <raylib.h>
#include <stdio.h>
#include <unistd.h>
#include <vlc/vlc.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
  float brightness;
  float volume;
} Settings;

typedef struct {
  NMClient *client;
  GHashTable *best_aps;
  bool scan_complete;
} WifiState;

static WifiState wifi = {0};

static void wifi_init() {
  // init wifi client
  GError *error = NULL;
  wifi.client = nm_client_new(NULL, &error);
  if (!wifi.client) {
    fprintf(stderr, "NMClient error: %s\n", error->message);
    g_error_free(error);
    return;
  }
  wifi.best_aps = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  wifi.scan_complete = false;
}

static void wifi_scan_done_cb(GObject *source, GAsyncResult *res,
                              gpointer user_data) {
  GError *error = NULL;

  if (!nm_device_wifi_request_scan_finish(NM_DEVICE_WIFI(source), res,
                                          &error)) {
    if (error) {
      fprintf(stderr, "WiFi scan failed: %s\n", error->message);
      g_error_free(error);
    }
  }
  // Signal render thread that scan results are ready
  wifi.scan_complete = true;
}

static void wifi_scan() {
  const GPtrArray *devices = nm_client_get_devices(wifi.client);
  g_hash_table_remove_all(wifi.best_aps);
  g_hash_table_destroy(wifi.best_aps);
  wifi.best_aps =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);

  for (guint i = 0; i < devices->len; i++) {
    NMDevice *dev = devices->pdata[i];
    if (!NM_IS_DEVICE_WIFI(dev))
      continue;
    GError *error = NULL;

    wifi.scan_complete = false;
    nm_device_wifi_request_scan_async(NM_DEVICE_WIFI(dev), NULL,
                                      wifi_scan_done_cb, NULL);

    NMDeviceWifi *wifi_dev = NM_DEVICE_WIFI(dev);
    const GPtrArray *aps = nm_device_wifi_get_access_points(wifi_dev);
    for (guint j = 0; j < aps->len; j++) {
      NMAccessPoint *ap = aps->pdata[j];
      GBytes *ssid_bytes = nm_access_point_get_ssid(ap);
      if (!ssid_bytes)
        continue;

      gsize len;
      const char *data = g_bytes_get_data(ssid_bytes, &len);
      char *ssid = g_strndup(data, len);

      NMAccessPoint *best = g_hash_table_lookup(wifi.best_aps, ssid);
      if (!best ||
          nm_access_point_get_strength(ap) > nm_access_point_get_strength(best))
        g_hash_table_insert(wifi.best_aps, ssid, g_object_ref(ap));
      else
        g_free(ssid);
    }
  }
}

void draw_settings(Settings *settings) {
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
  int WIFI_Y = SLIDER_Y + NEXT * 3.5;

  GuiPanel((Rectangle){PANEL_X, PANEL_Y, PANEL_WIDTH, PANEL_HEIGHT},
           "#141#Settings");
  GuiSliderBar((Rectangle){SLIDER_X, SLIDER_Y, SLIDER_WIDTH, SLIDER_HEIGHT},
               "Brightness", "", &settings->brightness, 0, 100);
  GuiSliderBar(
      (Rectangle){SLIDER_X, SLIDER_Y + NEXT, SLIDER_WIDTH, SLIDER_HEIGHT},
      "Volume    ", "", &settings->volume, 0, 100);
  GuiLine((Rectangle){PANEL_X, SLIDER_Y + NEXT * 2.5, PANEL_WIDTH, 0},
          "#189#WiFi");

  if (GuiButton((Rectangle){WIFI_X, WIFI_Y, 80, WIFI_HEIGHT}, "Scan")) {
    wifi_scan();
  }

  if (wifi.scan_complete) {
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
}

static void pump_glib(void) {
  while (g_main_context_iteration(NULL, FALSE))
    ;
}

int main() {

  const unsigned int frame_width = 800, frame_height = 480;
  libvlc_instance_t *vlc_instance;
  char const *vlc_argv[] = {"--no-audio", "--no-xlib"};
  Settings settings = {100.0, 100.0};
  wifi_init();

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
    // updating the texture if we have new data from VLC
    // maintains original FPS of the video
    pump_glib();
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
      draw_settings(&settings);

    EndDrawing();
  }
  UnloadTexture(video->texture);
  free(video->buffer);
  libvlc_release(vlc_instance);

  return 0;
}
