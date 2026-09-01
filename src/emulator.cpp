#include "emulator.h"
#include "log.h"
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
  char mac[18];
  const char *profile = node->kind == NODE_KIND_REGULAR
                            ? regular_profile_name((RegularProfile)node->profile)
                            : lite_profile_name((LiteProfile)node->profile);

  format_mac(node->mac, mac, sizeof(mac));

  if (kind[0] == 'd')
  {
    if (node->pending_data_ack && node->data_retry_count > 0)
    {
      log_msg_hex(pkt->data, pkt->size,
                  "TX %s  node %u  %s/%s  MAC %s  counter %u  retry %u/%u", kind, node->id,
                  node_kind_name(node->kind), profile, mac, node->pkt_counter,
                  node->data_retry_count, DATA_ACK_MAX_RETRIES);
    }
    else
    {
      log_msg_hex(pkt->data, pkt->size, "TX %s  node %u  %s/%s  MAC %s  counter %u", kind,
                  node->id, node_kind_name(node->kind), profile, mac, node->pkt_counter);
    }
  }
  else
  {
    log_msg_hex(pkt->data, pkt->size, "TX %s  node %u  %s/%s  MAC %s  attempt %u", kind,
                node->id, node_kind_name(node->kind), profile, mac, node->boot_attempts);
  }
}

static void log_schedule(const Node *node, const char *reason)
{
  uint32_t now = millis();
  int32_t wait_ms = (int32_t)(node->next_comm_ms - now);

  LOG("%s -> node %u dev#%u wakeup_unix=%lu in %ld s", reason, node->id, node->device_number,
      (unsigned long)node->next_wakeup_unix, (long)(wait_ms > 0 ? (wait_ms / 1000) : 0));
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

  log_msg_hex(rx.data, rx.size, "RX %u B", rx.size);

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

  LOG("RX ignored (unknown or unmatched ACK)");
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
  char mac_first[18];
  char mac_last[18];

  format_mac(nodes_get(0)->mac, mac_first, sizeof(mac_first));
  format_mac(nodes_get(nodes_count() - 1)->mac, mac_last, sizeof(mac_last));

  LOG("LoRa multi-node emulator");
  LOG("nodes: %u", nodes_count());
  LOG("regular: %u  lite: %u", REGULAR_NODE_COUNT, LITE_NODE_COUNT);
  LOG("MAC: %s .. %s", mac_first, mac_last);
  LOG("boot retry: %lu s / node", BOOT_INTERVAL_MS / 1000UL);
  LOG("boot ack wait (Regular): %lu s max (or sooner on ACK)", BOOT_ACK_WAIT_REGULAR_MS / 1000UL);
  LOG("sleep/time-slot: %u s / %u s", NODE_SLEEP_TIME_SEC, NODE_TIME_SLOT_SEC);
  LOG("tx gap: %lu s", TX_GAP_MS / 1000UL);
  if (time_is_synced())
    LOG("rtc: synced, unix=%lu", (unsigned long)current_unix_time());
  else
    LOG("rtc: waiting for first gateway frame");
  LOG("data ack retry: %lu s, max %u", DATA_ACK_RETRY_MS / 1000UL, DATA_ACK_MAX_RETRIES);
  LOG("next comm: (ActTime/Sleep+1)*Sleep + Dev#*Slot");
}

void emulator_tick()
{
  process_rx();
  service_tx();
}
