#include "settings.h"
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#include <libnm/NetworkManager.h>
#include <stdio.h>

Settings settings = {100.0, 100.0, false};
Wifi wifi = {0};

void on_nm_state_changed(NMClient *client, GParamSpec *pspec,
                         gpointer user_data) {
  wifi.state = nm_client_get_state(wifi.client);
  NMAccessPoint *ap =
      nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(wifi.device));
  if (ap != NULL) {
    wifi.current_ap = ap;
  }
}

void wifi_init() {
  // init wifi client
  GError *error = NULL;
  wifi.client = nm_client_new(NULL, &error);
  if (!wifi.client) {
    printf("NMClient error: %s\n", error->message);
    g_error_free(error);
    return;
  }
  const GPtrArray *devices = nm_client_get_devices(wifi.client);
  for (guint i = 0; i < devices->len; i++) {
    NMDevice *dev = devices->pdata[i];
    if (!NM_IS_DEVICE_WIFI(dev))
      continue;
    GError *error = NULL;
    wifi.device = dev;
    break;
  }
  wifi.best_aps = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  on_nm_state_changed(wifi.client, NULL, NULL);

  g_signal_connect(wifi.client, "notify::state",
                   G_CALLBACK(on_nm_state_changed), NULL);
}

void wifi_scan() {
  const GPtrArray *devices = nm_client_get_devices(wifi.client);
  g_hash_table_remove_all(wifi.best_aps);
  g_hash_table_destroy(wifi.best_aps);
  wifi.best_aps =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
  NMDeviceWifi *wifi_dev = NM_DEVICE_WIFI(wifi.device);
  nm_device_wifi_request_scan_async(wifi_dev, NULL, NULL, NULL);
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
