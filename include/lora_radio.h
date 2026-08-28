#pragma once

#include "packets.h"

bool radio_init();
void radio_rx_mode();
void radio_send(const lora_packet *pkt);
bool radio_poll_rx(lora_packet *pkt);
uint32_t radio_last_tx_ms();
bool radio_tx_gap_elapsed();
