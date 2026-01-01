#include "video.h"
#include <stdlib.h>

static void *begin_vlc_rendering(void *data, void **p_pixels) {
  // Lock pixels. Wait for vlc to draw a frame inside.
  Video *video = (Video *)data;
  pthread_mutex_lock(&video->mutex);
  *p_pixels = video->buffer;
  return video; // Not used
}

static void display_vlc_rendering(void *data, void *id) {
  // Nothing to do — Raylib draws later
  (void)data;
  (void)id;
}

static void end_vlc_rendering(void *data, void *id, void *const *p_pixels) {
  // Frame drawn. Unlock pixels.
  Video *video = (Video *)data;
  video->needUpdate = true;
  pthread_mutex_unlock(&video->mutex);
}

Video *make_video_from_path(libvlc_instance_t *vlc_instance, char *path,
                            unsigned int width, unsigned int height) {
  Video *video = malloc(sizeof(Video));
  pthread_mutex_init(&video->mutex, NULL);
  libvlc_media_t *media = libvlc_media_new_path(vlc_instance, path);
  video->player = libvlc_media_player_new_from_media(media);
  libvlc_media_release(media);
  video->needUpdate = false;
  size_t frame_size = width * height * 3; // RGB: 3 bytes per pixel
  video->buffer = MemAlloc(frame_size);
  libvlc_video_set_format(video->player, "RV24", width, height, width * 3);
  libvlc_video_set_callbacks(video->player, begin_vlc_rendering,
                             end_vlc_rendering, display_vlc_rendering, video);
  return video;
}
