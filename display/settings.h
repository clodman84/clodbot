#ifndef SETTINGS_H
#define SETTINGS_H

#include <glib.h>
#include <libnm/NetworkManager.h>
#include <stdbool.h>

typedef struct {
  float brightness;
  float volume;
  bool changed;
} Settings;

typedef struct {
  char *ssid;
  char password[64];
  NMState state;
  NMDevice *device;
  NMClient *client;
  GHashTable *best_aps;
  NMAccessPoint *current_ap;
} Wifi;

extern Settings settings;
extern Wifi wifi;

void wifi_init();
void wifi_scan();

#endif // !WIFI_H
