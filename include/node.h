#pragma once

#include <Arduino.h>
#include "config.h"
#include "profiles.h"

enum NodeState : uint8_t
{
  NODE_UNJOINED = 0,
  NODE_JOINED,
};

struct Node
{
  uint8_t id;
  uint8_t mac[6];
  NodeKind kind;
  uint8_t profile;
  NodeState state;

  uint32_t boot_interval_ms;
  uint32_t next_boot_ms;

  uint32_t next_comm_ms;
  uint32_t next_wakeup_unix;

  uint16_t sleep_time_sec;
  uint8_t time_slot_sec;
  uint8_t device_number;
  bool pending_data_ack;
  uint8_t data_retry_count;
  uint32_t data_retry_at_ms;

  uint16_t pkt_counter;
  uint8_t boot_attempts;

  // Fixed per-node IO states (Regular REG_PROFILE_BLE_IO)
  uint8_t door;
  uint8_t water;
};

void nodes_init();
void time_init();
void time_sync_from_gateway_once(uint32_t gateway_unix);
bool time_is_synced();
uint8_t nodes_count();
Node *nodes_get(uint8_t index);
Node *nodes_find_unjoined_mac_in(const uint8_t *payload, uint8_t len);
Node *nodes_find_mac_in(const uint8_t *payload, uint8_t len);

bool node_boot_due(const Node *node, uint32_t now);
bool node_data_due(const Node *node, uint32_t now);
bool node_data_retry_due(const Node *node, uint32_t now);
bool node_data_retries_exhausted(const Node *node, uint32_t now);
void node_on_boot_sent(Node *node);
void node_on_joined(Node *node, uint32_t act_time, uint16_t sleep_sec, uint8_t slot_sec,
                    uint8_t device_number);
void node_on_data_ack(Node *node, uint32_t act_time, uint16_t sleep_sec, uint8_t slot_sec,
                      uint8_t device_number);
void node_on_data_cycle_started(Node *node);
void node_on_data_retry(Node *node);
void node_on_data_retries_exhausted(Node *node);

uint32_t node_calc_wakeup_unix(uint32_t act_time, uint16_t sleep_sec, uint8_t slot_sec,
                               uint8_t device_number);

void print_mac(const uint8_t *mac);
void format_mac(const uint8_t *mac, char *out, size_t out_len);
uint32_t current_unix_time();
