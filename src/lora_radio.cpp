#include "lora_radio.h"
#include "config.h"
#include <SPI.h>
#include <LoRa.h>
#include <string.h>

static volatile bool rx_pending = false;
static uint8_t rx_buf[128];
static volatile uint8_t rx_len = 0;
static uint32_t last_tx_ms = 0;
static bool has_tx = false;

static void on_receive(int packetSize)
{
  if (packetSize <= 0)
    return;

  uint8_t n = (uint8_t)packetSize;
  if (n > sizeof(rx_buf))
    n = sizeof(rx_buf);

  for (uint8_t i = 0; i < n; i++)
    rx_buf[i] = (uint8_t)LoRa.read();
  while (LoRa.available())
    LoRa.read();

  rx_len = n;
  rx_pending = true;
}

bool radio_init()
{
  SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_CS_PIN);
  LoRa.setPins(LORA_CS_PIN, LORA_RESET_PIN, LORA_IRQ_PIN);

  if (!LoRa.begin(LORA_FREQUENCY))
    return false;

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.onReceive(on_receive);
  radio_rx_mode();
  return true;
}

void radio_rx_mode()
{
  LoRa.disableInvertIQ();
  LoRa.receive();
}

void radio_send(const lora_packet *pkt)
{
  LoRa.idle();
  LoRa.disableInvertIQ();
  LoRa.beginPacket();
  LoRa.write(pkt->data, pkt->size);
  LoRa.endPacket(false);
  last_tx_ms = millis();
  has_tx = true;
  radio_rx_mode();
}

bool radio_poll_rx(lora_packet *pkt)
{
  if (!rx_pending)
    return false;

  uint8_t len = rx_len;
  if (len > sizeof(pkt->data))
    len = sizeof(pkt->data);
  memcpy(pkt->data, (const void *)rx_buf, len);
  pkt->size = len;
  rx_pending = false;
  return true;
}

uint32_t radio_last_tx_ms()
{
  return last_tx_ms;
}

bool radio_tx_gap_elapsed()
{
  if (!has_tx)
    return true;
  return (millis() - last_tx_ms) >= TX_GAP_MS;
}
