#pragma once

#include <Arduino.h>
#include "node.h"

struct lora_packet
{
  uint8_t data[100];
  uint8_t size;
};

void prepare_bootup_packet(const Node *node, lora_packet *pkt);
void prepare_data_packet(const Node *node, lora_packet *pkt);
