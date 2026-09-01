#include "node.h"
#include "log.h"
#include "profiles.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static Node g_nodes[NODE_COUNT_MAX];
static uint8_t g_node_count = NODE_COUNT;
static uint32_t g_boot_unix = UNIX_TIME_BASE;
static uint32_t g_boot_millis = 0;
static bool g_rtc_synced = false;

void time_init()
{
  g_boot_unix = UNIX_TIME_BASE;
  g_boot_millis = millis();
  g_rtc_synced = false;
  setenv("TZ", "UTC0", 1);
  tzset();
}

void time_sync_from_gateway_once(uint32_t gateway_unix)
{
  if (g_rtc_synced)
    return;

  g_boot_unix = gateway_unix;
  g_boot_millis = millis();

  struct timeval tv;
  tv.tv_sec = (time_t)gateway_unix;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  g_rtc_synced = true;

  LOG("RTC set from gateway unix=%lu", (unsigned long)gateway_unix);
}

bool time_is_synced()
{
  return g_rtc_synced;
}

static uint32_t read_le32(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static void schedule_wakeup(Node *node, uint32_t act_time, uint16_t sleep_sec, uint8_t slot_sec,
                            uint8_t device_number)
{
  node->sleep_time_sec = sleep_sec;
  node->time_slot_sec = slot_sec;
  node->device_number = device_number;
  node->next_wakeup_unix =
      node_calc_wakeup_unix(act_time, sleep_sec, slot_sec, device_number);

  int32_t delta_sec = (int32_t)(node->next_wakeup_unix - act_time);
  if (delta_sec < 0)
    delta_sec = 0;

  node->next_comm_ms = millis() + (uint32_t)delta_sec * 1000UL;
  node->pending_data_ack = false;
  node->data_retry_count = 0;
}

void nodes_init()
{
  if (g_node_count < 1)
    g_node_count = 1;
  if (g_node_count > NODE_COUNT_MAX)
    g_node_count = NODE_COUNT_MAX;

  randomSeed((uint32_t)micros());

  uint32_t now = millis();
  for (uint8_t i = 0; i < g_node_count; i++)
  {
    Node *n = &g_nodes[i];
    n->id = i + 1;
    n->mac[0] = MAC_OUI_0;
    n->mac[1] = MAC_OUI_1;
    n->mac[2] = MAC_OUI_2;
    n->mac[3] = MAC_OUI_3;
    n->mac[4] = MAC_OUI_4;
    n->mac[5] = n->id;
    n->state = NODE_UNJOINED;
    n->boot_interval_ms = BOOT_INTERVAL_MS;
    n->next_boot_ms = now;
    n->next_comm_ms = UINT32_MAX;
    n->next_wakeup_unix = 0;
    n->sleep_time_sec = NODE_SLEEP_TIME_SEC;
    n->time_slot_sec = NODE_TIME_SLOT_SEC;
    n->device_number = n->id;
    n->pending_data_ack = false;
    n->data_retry_count = 0;
    n->data_retry_at_ms = 0;
    n->pkt_counter = 0;
    n->boot_attempts = 0;
    n->door = 2;
    n->water = 2;
    node_apply_profile(n);
  }
}

uint8_t nodes_count()
{
  return g_node_count;
}

Node *nodes_get(uint8_t index)
{
  if (index >= g_node_count)
    return nullptr;
  return &g_nodes[index];
}

Node *nodes_find_unjoined_mac_in(const uint8_t *payload, uint8_t len)
{
  if (payload == nullptr || len < 6)
    return nullptr;

  for (uint8_t i = 0; i < g_node_count; i++)
  {
    Node *n = &g_nodes[i];
    if (n->state != NODE_UNJOINED)
      continue;
    for (uint8_t pos = 0; pos + 6 <= len; pos++)
    {
      if (memcmp(&payload[pos], n->mac, 6) == 0)
        return n;
    }
  }
  return nullptr;
}

Node *nodes_find_mac_in(const uint8_t *payload, uint8_t len)
{
  if (payload == nullptr || len < 6)
    return nullptr;

  for (uint8_t i = 0; i < g_node_count; i++)
  {
    Node *n = &g_nodes[i];
    for (uint8_t pos = 0; pos + 6 <= len; pos++)
    {
      if (memcmp(&payload[pos], n->mac, 6) == 0)
        return n;
    }
  }
  return nullptr;
}

bool node_boot_due(const Node *node, uint32_t now)
{
  return node != nullptr &&
         node->state == NODE_UNJOINED &&
         (int32_t)(now - node->next_boot_ms) >= 0;
}

bool node_data_due(const Node *node, uint32_t now)
{
  return node != nullptr &&
         node->state == NODE_JOINED &&
         !node->pending_data_ack &&
         (int32_t)(now - node->next_comm_ms) >= 0;
}

bool node_data_retry_due(const Node *node, uint32_t now)
{
  return node != nullptr &&
         node->state == NODE_JOINED &&
         node->pending_data_ack &&
         node->data_retry_count < DATA_ACK_MAX_RETRIES &&
         (int32_t)(now - node->data_retry_at_ms) >= 0;
}

bool node_data_retries_exhausted(const Node *node, uint32_t now)
{
  return node != nullptr &&
         node->state == NODE_JOINED &&
         node->pending_data_ack &&
         node->data_retry_count >= DATA_ACK_MAX_RETRIES &&
         (int32_t)(now - node->data_retry_at_ms) >= 0;
}

void node_on_boot_sent(Node *node)
{
  if (node == nullptr)
    return;
  node->boot_attempts++;
  node->next_boot_ms = millis() + node->boot_interval_ms;
}

void node_on_joined(Node *node, uint32_t act_time, uint16_t sleep_sec, uint8_t slot_sec,
                    uint8_t device_number)
{
  if (node == nullptr)
    return;
  node->state = NODE_JOINED;
  schedule_wakeup(node, act_time, sleep_sec, slot_sec, device_number);
}

void node_on_data_ack(Node *node, uint32_t act_time, uint16_t sleep_sec, uint8_t slot_sec,
                      uint8_t device_number)
{
  if (node == nullptr)
    return;
  schedule_wakeup(node, act_time, sleep_sec, slot_sec, device_number);
}

void node_on_data_cycle_started(Node *node)
{
  if (node == nullptr)
    return;
  node->pkt_counter++;
  node->pending_data_ack = true;
  node->data_retry_count = 0;
  node->data_retry_at_ms = millis() + DATA_ACK_RETRY_MS;
}

void node_on_data_retry(Node *node)
{
  if (node == nullptr)
    return;
  node->data_retry_count++;
  node->data_retry_at_ms = millis() + DATA_ACK_RETRY_MS;
}

void node_on_data_retries_exhausted(Node *node)
{
  if (node == nullptr)
    return;
  LOG("DATA ACK timeout node %u after %u retries, scheduling fallback", node->id,
      DATA_ACK_MAX_RETRIES);
  node->pending_data_ack = false;
  node->data_retry_count = 0;
  schedule_wakeup(node, current_unix_time(), node->sleep_time_sec, node->time_slot_sec,
                  node->device_number);
}

uint32_t node_calc_wakeup_unix(uint32_t act_time, uint16_t sleep_sec, uint8_t slot_sec,
                               uint8_t device_number)
{
  uint32_t bucket = act_time / sleep_sec;
  return (bucket + 1U) * sleep_sec + (uint32_t)device_number * (uint32_t)slot_sec;
}

void format_mac(const uint8_t *mac, char *out, size_t out_len)
{
  snprintf(out, out_len, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
           mac[4], mac[5]);
}

void print_mac(const uint8_t *mac)
{
  char buf[18];

  format_mac(mac, buf, sizeof(buf));
  Serial.print(buf);
}

uint32_t current_unix_time()
{
  uint32_t elapsed = (millis() - g_boot_millis) / 1000UL;
  return g_boot_unix + elapsed;
}
