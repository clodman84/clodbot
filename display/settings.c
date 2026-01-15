#include "settings.h"
#include <ctype.h>
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#include <libnm/NetworkManager.h>
#include <nm-core-types.h>
#include <stdio.h>

Settings settings = {100.0, 100.0, false};
Wifi wifi = {0};

char *get_ssid(NMAccessPoint *ap) {
  GBytes *ssid_bytes = nm_access_point_get_ssid(ap);
  gsize len;
  const char *data = g_bytes_get_data(ssid_bytes, &len);
  char *ssid = g_strndup(data, len);
  return ssid;
}

void on_nm_state_changed(NMClient *client, GParamSpec *pspec,
                         gpointer user_data) {
  wifi.state = nm_client_get_state(wifi.client);
  NMAccessPoint *ap =
      nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(wifi.device));
  if (ap != NULL) {
    wifi.current_ap = ap;
    wifi.current_ssid = get_ssid(ap);
  }
}

static NMConnection *find_existing_wifi_connection(NMClient *client,
                                                   NMAccessPoint *ap);

static void on_device_state_changed(NMDevice *device, GParamSpec *pspec,
                                    gpointer user_data) {
  NMDeviceState state = nm_device_get_state(device);

  if (state != NM_DEVICE_STATE_FAILED)
    return;

  NMConnection *conn =
      find_existing_wifi_connection(wifi.client, wifi.attempt_ap);
  nm_remote_connection_delete_async(NM_REMOTE_CONNECTION(conn), NULL, NULL,
                                    NULL);
}

void wifi_init() {
  // init wifi client
  GError *error = NULL;
  wifi.client = nm_client_new(NULL, &error);
  wifi.show_connection_dialog = false;
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
  g_signal_connect(wifi.device, "notify::state",
                   G_CALLBACK(on_device_state_changed), NULL);
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

static NMConnection *find_existing_wifi_connection(NMClient *client,
                                                   NMAccessPoint *ap) {
  const GPtrArray *connections = nm_client_get_connections(client);
  GBytes *ap_ssid = nm_access_point_get_ssid(ap);
  for (guint i = 0; i < connections->len; i++) {
    NMConnection *conn = g_ptr_array_index(connections, i);
    NMSettingWireless *s_wifi = nm_connection_get_setting_wireless(conn);
    if (!s_wifi)
      continue;
    GBytes *ssid = nm_setting_wireless_get_ssid(s_wifi);
    if (ssid && g_bytes_equal(ssid, ap_ssid))
      return conn;
  }
  return NULL;
}

bool wifi_connect(NMAccessPoint *ap) {
  NMConnection *existing = find_existing_wifi_connection(wifi.client, ap);
  if (existing) {
    const char *specific_object = nm_object_get_path(NM_OBJECT(ap));
    nm_client_activate_connection_async(wifi.client, existing, wifi.device,
                                        specific_object, NULL, NULL, NULL);
    return true;
  }
  return false;
}

void wifi_new_connection() {
  printf("Password: %s\n", wifi.password);
  NM80211ApFlags flags = nm_access_point_get_flags(wifi.attempt_ap);
  bool open = !(flags & NM_802_11_AP_FLAGS_PRIVACY);
  NMConnection *conn = nm_simple_connection_new();
  NMSettingConnection *s_con =
      NM_SETTING_CONNECTION(nm_setting_connection_new());
  nm_connection_add_setting(conn, NM_SETTING(s_con));

  g_object_set(s_con, NM_SETTING_CONNECTION_TYPE,
               NM_SETTING_WIRELESS_SETTING_NAME, NM_SETTING_CONNECTION_ID,
               nm_access_point_get_ssid(wifi.attempt_ap),
               NM_SETTING_CONNECTION_AUTOCONNECT, TRUE, NULL);

  NMSettingWireless *s_wifi = NM_SETTING_WIRELESS(nm_setting_wireless_new());
  nm_connection_add_setting(conn, NM_SETTING(s_wifi));

  g_object_set(s_wifi, NM_SETTING_WIRELESS_SSID,
               nm_access_point_get_ssid(wifi.attempt_ap),
               NM_SETTING_WIRELESS_MODE, "infrastructure", NULL);

  if (!open) {
    NMSettingWirelessSecurity *s_sec =
        NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());

    g_object_set(s_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-psk",
                 NM_SETTING_WIRELESS_SECURITY_PSK, wifi.password, NULL);

    nm_connection_add_setting(conn, NM_SETTING(s_sec));
  }

  const char *specific_object = nm_object_get_path(NM_OBJECT(wifi.attempt_ap));
  nm_client_add_and_activate_connection_async(
      wifi.client, conn, wifi.device, specific_object, NULL, NULL, NULL);

  g_object_unref(conn);
}
