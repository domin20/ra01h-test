#include "emulator.h"
#include "node.h"
#include "packets.h"
#include "lora_radio.h"

static Node *last_boot_node = nullptr;
static lora_packet tx_packet;

static void log_tx(const char *kind, const Node *node, const lora_packet *pkt)
{
  Serial.print("TX ");
  Serial.print(kind);
  Serial.print("  node ");
  Serial.print(node->id);
  Serial.print("  MAC ");
  print_mac(node->mac);
  if (kind[0] == 'd')
  {
    Serial.print("  counter ");
    Serial.print(node->pkt_counter);
  }
  else
  {
    Serial.print("  attempt ");
    Serial.print(node->boot_attempts);
  }
  Serial.print("  ");
  dump_hex(pkt->data, pkt->size);
}

static void send_boot(Node *node)
{
  prepare_bootup_packet(node, &tx_packet);
  radio_send(&tx_packet);
  node_on_boot_sent(node);
  last_boot_node = node;
  log_tx("boot", node, &tx_packet);
}

static void send_data(Node *node)
{
  node_on_data_sent(node);
  prepare_data_packet(node, &tx_packet);
  radio_send(&tx_packet);
  log_tx("data", node, &tx_packet);
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

  Node *node = nodes_find_unjoined_mac_in(rx.data, rx.size);
  if (node == nullptr && last_boot_node != nullptr &&
      last_boot_node->state == NODE_UNJOINED)
  {
    node = last_boot_node;
    Serial.println("  ACK matched by last boot (no MAC in payload)");
  }

  if (node == nullptr)
  {
    Serial.println("  RX ignored (no waiting node)");
    return;
  }

  node_on_joined(node);
  uint32_t now = millis();
  int32_t wait_ms = (int32_t)(node->next_comm_ms - now);

  Serial.print("  ACK -> node ");
  Serial.print(node->id);
  Serial.print(" joined, next data in ");
  Serial.print(wait_ms > 0 ? (wait_ms / 1000) : 0);
  Serial.println(" s");
}

static void service_tx()
{
  if (!radio_tx_gap_elapsed())
    return;

  uint32_t now = millis();
  const uint8_t count = nodes_count();

  for (uint8_t i = 0; i < count; i++)
  {
    Node *n = nodes_get(i);
    if (node_data_due(n, now))
    {
      send_data(n);
      return;
    }
  }

  for (uint8_t i = 0; i < count; i++)
  {
    Node *n = nodes_get(i);
    if (node_boot_due(n, now))
    {
      send_boot(n);
      return;
    }
  }
}

void emulator_start()
{
  Serial.println("LoRa multi-node emulator");
  Serial.print("  nodes: ");
  Serial.println(nodes_count());
  Serial.print("  variant: ");
  Serial.println(NODE_VARIANT == 0 ? "Regular" : "NodeLite");
  Serial.print("  MAC: ");
  print_mac(nodes_get(0)->mac);
  Serial.print(" .. ");
  print_mac(nodes_get(nodes_count() - 1)->mac);
  Serial.println();
  Serial.print("  boot retry: ");
  Serial.print(BOOT_INTERVAL_MS / 1000);
  Serial.println(" s / node");
  Serial.print("  data interval: ");
  Serial.print(COMM_INTERVAL_MS / 1000);
  Serial.println(" s");
  Serial.print("  tx gap: ");
  Serial.print(TX_GAP_MS / 1000);
  Serial.println(" s");
}

void emulator_tick()
{
  process_rx();
  service_tx();
}
