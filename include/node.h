#pragma once

#include <Arduino.h>
#include "config.h"

enum NodeState : uint8_t
{
  NODE_UNJOINED = 0,
  NODE_JOINED,
};

struct Node
{
  uint8_t id;
  uint8_t mac[6];
  NodeState state;

  uint32_t boot_interval_ms;
  uint32_t next_boot_ms;

  uint32_t comm_interval_ms;
  uint32_t next_comm_ms;

  uint16_t pkt_counter;
  uint8_t boot_attempts;
};

void nodes_init();
uint8_t nodes_count();
Node *nodes_get(uint8_t index);
Node *nodes_find_unjoined_mac_in(const uint8_t *payload, uint8_t len);

bool node_boot_due(const Node *node, uint32_t now);
bool node_data_due(const Node *node, uint32_t now);
void node_on_boot_sent(Node *node);
void node_on_joined(Node *node);
void node_on_data_sent(Node *node);

void print_mac(const uint8_t *mac);
void dump_hex(const uint8_t *data, uint8_t len);
uint32_t current_unix_time();
