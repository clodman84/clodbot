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
  char *current_ssid;
  char password[64];
  NMState state;
  NMDevice *device;
  NMClient *client;
  GHashTable *best_aps;
  NMAccessPoint *current_ap;
  NMAccessPoint *attempt_ap;
  bool show_connection_dialog;
} Wifi;

extern Settings settings;
extern Wifi wifi;

void wifi_init();
void wifi_scan();
bool wifi_connect(NMAccessPoint *ap);
void wifi_new_connection();
char *get_ssid(NMAccessPoint *ap);

#endif // !WIFI_H
