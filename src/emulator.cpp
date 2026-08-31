#include "emulator.h"
#include "node.h"
#include "packets.h"
#include "profiles.h"
#include "lora_radio.h"

static Node *last_boot_node = nullptr;
static Node *last_data_node = nullptr;
static lora_packet tx_packet;
static bool boot_awaiting_ack = false;
static uint32_t boot_tx_at_ms = 0;

static uint32_t read_le32(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const uint8_t *p)
{
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void log_tx(const char *kind, const Node *node, const lora_packet *pkt)
{
  Serial.print("TX ");
  Serial.print(kind);
  Serial.print("  node ");
  Serial.print(node->id);
  Serial.print("  ");
  Serial.print(node_kind_name(node->kind));
  Serial.print("/");
  if (node->kind == NODE_KIND_REGULAR)
    Serial.print(regular_profile_name((RegularProfile)node->profile));
  else
    Serial.print(lite_profile_name((LiteProfile)node->profile));
  Serial.print("  MAC ");
  print_mac(node->mac);
  if (kind[0] == 'd')
  {
    Serial.print("  counter ");
    Serial.print(node->pkt_counter);
    if (node->pending_data_ack && node->data_retry_count > 0)
    {
      Serial.print("  retry ");
      Serial.print(node->data_retry_count);
      Serial.print("/");
      Serial.print(DATA_ACK_MAX_RETRIES);
    }
  }
  else
  {
    Serial.print("  attempt ");
    Serial.print(node->boot_attempts);
  }
  Serial.print("  ");
  dump_hex(pkt->data, pkt->size);
}

static void log_schedule(const Node *node, const char *reason)
{
  uint32_t now = millis();
  int32_t wait_ms = (int32_t)(node->next_comm_ms - now);

  Serial.print("  ");
  Serial.print(reason);
  Serial.print(" -> node ");
  Serial.print(node->id);
  Serial.print(" dev#");
  Serial.print(node->device_number);
  Serial.print(" wakeup_unix=");
  Serial.print(node->next_wakeup_unix);
  Serial.print(" in ");
  Serial.print(wait_ms > 0 ? (wait_ms / 1000) : 0);
  Serial.println(" s");
}

static bool parse_boot_ack(const lora_packet *rx, Node **node_out, uint32_t *act_time_out,
                           uint16_t *sleep_sec_out, uint8_t *slot_sec_out,
                           uint8_t *device_number_out)
{
  if (rx->size < 17 || rx->data[0] != '&')
    return false;

  Node *node = nodes_find_mac_in(&rx->data[1], 6);
  if (node == nullptr && last_boot_node != nullptr)
    node = last_boot_node;
  if (node == nullptr)
    return false;

  *node_out = node;
  *slot_sec_out = rx->data[7];
  *sleep_sec_out = read_le16(&rx->data[8]);
  *device_number_out = rx->data[10];
  *act_time_out = read_le32(&rx->data[11]);
  return true;
}

static bool parse_regular_data_ack(const lora_packet *rx, Node **node_out, uint32_t *act_time_out,
                                   uint16_t *sleep_sec_out, uint8_t *slot_sec_out)
{
  if (rx->size < 16 || rx->data[0] != 'C')
    return false;

  Node *node = nodes_find_mac_in(&rx->data[1], 6);
  if (node == nullptr && last_data_node != nullptr)
    node = last_data_node;
  if (node == nullptr || node->state != NODE_JOINED)
    return false;

  *node_out = node;
  *act_time_out = read_le32(&rx->data[7]);
  *slot_sec_out = rx->data[11];
  *sleep_sec_out = read_le16(&rx->data[12]);
  return true;
}

static bool parse_lite_data_ack(const lora_packet *rx, Node **node_out, uint32_t *act_time_out,
                                 uint16_t *sleep_sec_out, uint8_t *slot_sec_out,
                                 uint8_t *device_number_out)
{
  if (rx->size < 29 || rx->data[0] != '=')
    return false;
  if (rx->data[7] != 'O' || rx->data[8] != 'K')
    return false;

  Node *node = nodes_find_mac_in(&rx->data[1], 6);
  if (node == nullptr && last_data_node != nullptr)
    node = last_data_node;
  if (node == nullptr || node->state != NODE_JOINED)
    return false;

  *node_out = node;
  *slot_sec_out = rx->data[9];
  *sleep_sec_out = read_le16(&rx->data[10]);
  *device_number_out = rx->data[12];
  *act_time_out = read_le32(&rx->data[23]);
  return true;
}

static void send_data_frame(Node *node, bool is_retry)
{
  if (!is_retry)
    node_on_data_cycle_started(node);
  else
    node_on_data_retry(node);

  prepare_data_packet(node, &tx_packet);
  radio_send(&tx_packet);
  last_data_node = node;
  log_tx(is_retry ? "data-retry" : "data", node, &tx_packet);
}

static void send_boot(Node *node)
{
  prepare_bootup_packet(node, &tx_packet);
  radio_send(&tx_packet);
  node_on_boot_sent(node);
  last_boot_node = node;
  log_tx("boot", node, &tx_packet);
}

static void process_rx()
{
  lora_packet rx;
  if (!radio_poll_rx(&rx))
    return;

  Serial.print("RX ");
  Serial.print(rx.size);
  Serial.print(" B  ");
  dump_hex(rx.data, rx.size);

  Node *node = nullptr;
  uint32_t act_time = 0;
  uint16_t sleep_sec = NODE_SLEEP_TIME_SEC;
  uint8_t slot_sec = NODE_TIME_SLOT_SEC;
  uint8_t device_number = 0;

  if (parse_boot_ack(&rx, &node, &act_time, &sleep_sec, &slot_sec, &device_number))
  {
    boot_awaiting_ack = false;
    time_sync_from_gateway_once(act_time);
    if (node->state == NODE_UNJOINED)
    {
      node_on_joined(node, act_time, sleep_sec, slot_sec, device_number);
      log_schedule(node, "BOOT ACK");
    }
    return;
  }

  if (parse_regular_data_ack(&rx, &node, &act_time, &sleep_sec, &slot_sec))
  {
    time_sync_from_gateway_once(act_time);
    device_number = node->device_number;
    node_on_data_ack(node, act_time, sleep_sec, slot_sec, device_number);
    log_schedule(node, "DATA ACK (Regular)");
    return;
  }

  if (parse_lite_data_ack(&rx, &node, &act_time, &sleep_sec, &slot_sec, &device_number))
  {
    time_sync_from_gateway_once(act_time);
    node_on_data_ack(node, act_time, sleep_sec, slot_sec, device_number);
    log_schedule(node, "DATA ACK (NodeLite)");
    return;
  }

  Serial.println("  RX ignored (unknown or unmatched ACK)");
}

static void service_tx()
{
  if (!radio_tx_gap_elapsed())
    return;

  uint32_t now = millis();

  if (boot_awaiting_ack)
  {
    if ((int32_t)(now - boot_tx_at_ms) < (int32_t)BOOT_ACK_WAIT_REGULAR_MS)
      return;
    boot_awaiting_ack = false;
  }
  const uint8_t count = nodes_count();

  for (uint8_t i = 0; i < count; i++)
  {
    Node *n = nodes_get(i);
    if (node_data_retries_exhausted(n, now))
    {
      node_on_data_retries_exhausted(n);
      log_schedule(n, "DATA ACK timeout");
      return;
    }
  }

  for (uint8_t i = 0; i < count; i++)
  {
    Node *n = nodes_get(i);
    if (node_data_retry_due(n, now))
    {
      send_data_frame(n, true);
      return;
    }
  }

  for (uint8_t i = 0; i < count; i++)
  {
    Node *n = nodes_get(i);
    if (node_data_due(n, now))
    {
      send_data_frame(n, false);
      return;
    }
  }

  for (uint8_t i = 0; i < count; i++)
  {
    Node *n = nodes_get(i);
    if (node_boot_due(n, now))
    {
      send_boot(n);
      if (n->kind == NODE_KIND_REGULAR)
      {
        boot_awaiting_ack = true;
        boot_tx_at_ms = now;
      }
      return;
    }
  }
}

void emulator_start()
{
  Serial.println("LoRa multi-node emulator");
  Serial.print("  nodes: ");
  Serial.println(nodes_count());
  Serial.print("  regular: ");
  Serial.print(REGULAR_NODE_COUNT);
  Serial.print("  lite: ");
  Serial.println(LITE_NODE_COUNT);
  Serial.print("  MAC: ");
  print_mac(nodes_get(0)->mac);
  Serial.print(" .. ");
  print_mac(nodes_get(nodes_count() - 1)->mac);
  Serial.println();
  Serial.print("  boot retry: ");
  Serial.print(BOOT_INTERVAL_MS / 1000);
  Serial.println(" s / node");
  Serial.print("  boot ack wait (Regular): ");
  Serial.print(BOOT_ACK_WAIT_REGULAR_MS / 1000);
  Serial.println(" s max (or sooner on ACK)");
  Serial.print("  sleep/time-slot: ");
  Serial.print(NODE_SLEEP_TIME_SEC);
  Serial.print(" s / ");
  Serial.print(NODE_TIME_SLOT_SEC);
  Serial.println(" s");
  Serial.print("  tx gap: ");
  Serial.print(TX_GAP_MS / 1000);
  Serial.println(" s");
  Serial.print("  rtc: ");
  if (time_is_synced())
  {
    Serial.print("synced, unix=");
    Serial.println(current_unix_time());
  }
  else
  {
    Serial.println("waiting for first gateway frame");
  }
  Serial.print("  data ack retry: ");
  Serial.print(DATA_ACK_RETRY_MS / 1000);
  Serial.print(" s, max ");
  Serial.println(DATA_ACK_MAX_RETRIES);
  Serial.println("  next comm: (ActTime/Sleep+1)*Sleep + Dev#*Slot");
}

void emulator_tick()
{
  process_rx();
  service_tx();
}
