#ifndef VIDEO_H

#define VIDEO_H

#include <pthread.h>
#include <raylib.h>
#include <vlc/vlc.h>

typedef struct {
  pthread_mutex_t mutex; // Mutex to begin_vlc_rendering texture on drawing
  Texture2D texture;     // Here we draw the pixel from vlc
  uint8_t *buffer;       // Pixel received from vlc
  bool needUpdate;       // Texture is changed, we need to reload
  libvlc_media_player_t *player; // The mediaplayer
  unsigned int width, height;
} Video;

Video *make_video_from_path(libvlc_instance_t *vlc_instance, char *path,
                            unsigned int width, unsigned int height);

#endif // !VIDEO_H
