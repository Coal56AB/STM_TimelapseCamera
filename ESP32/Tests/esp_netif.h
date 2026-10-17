#pragma once
#include "WiFi.h"
enum esp_netif_dhcp_status_t { ESP_NETIF_DHCP_STOPPED, ESP_NETIF_DHCP_STARTED };
inline int esp_netif_dhcps_get_status(void *, esp_netif_dhcp_status_t *status) {
  *status = WiFi.dhcpRunning ? ESP_NETIF_DHCP_STARTED : ESP_NETIF_DHCP_STOPPED;
  return 0;
}
