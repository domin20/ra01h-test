#include "node.h"
#include <string.h>

static Node g_nodes[NODE_COUNT_MAX];
static uint8_t g_node_count = NODE_COUNT;

void nodes_init()
{
  if (g_node_count < 1)
    g_node_count = 1;
  if (g_node_count > NODE_COUNT_MAX)
    g_node_count = NODE_COUNT_MAX;

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
    n->comm_interval_ms = COMM_INTERVAL_MS;
    n->next_comm_ms = now + n->comm_interval_ms;
    n->pkt_counter = 0;
    n->boot_attempts = 0;
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
         (int32_t)(now - node->next_comm_ms) >= 0;
}

void node_on_boot_sent(Node *node)
{
  if (node == nullptr)
    return;
  node->boot_attempts++;
  node->next_boot_ms = millis() + node->boot_interval_ms;
}

void node_on_joined(Node *node)
{
  if (node == nullptr)
    return;
  node->state = NODE_JOINED;
}

void node_on_data_sent(Node *node)
{
  if (node == nullptr)
    return;
  node->pkt_counter++;
  do
  {
    node->next_comm_ms += node->comm_interval_ms;
  } while ((int32_t)(millis() - node->next_comm_ms) >= 0);
}

void print_mac(const uint8_t *mac)
{
  for (uint8_t i = 0; i < 6; i++)
  {
    if (mac[i] < 16)
      Serial.print('0');
    Serial.print(mac[i], HEX);
    if (i < 5)
      Serial.print(':');
  }
}

void dump_hex(const uint8_t *data, uint8_t len)
{
  for (uint8_t i = 0; i < len; i++)
  {
    if (data[i] < 16)
      Serial.print('0');
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

uint32_t current_unix_time()
{
  return UNIX_TIME_BASE + (millis() / 1000UL);
}
