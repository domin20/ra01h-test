#pragma once

#define NODE_COUNT 5
#define NODE_COUNT_MAX 50

// 0 = Regular ('A' boot, 'R' data)
// 1 = NodeLite ('%' boot, '#' data)
#define NODE_VARIANT 0

#define BOOT_INTERVAL_MS 60000UL  // retry bootup for the same node
#define COMM_INTERVAL_MS 600000UL // data interval / first data delay
#define TX_GAP_MS 1000UL          // min spacing between any two frames

#define MAC_OUI_0 0x00
#define MAC_OUI_1 0x00
#define MAC_OUI_2 0x00
#define MAC_OUI_3 0x00
#define MAC_OUI_4 0x00

#define LORA_FREQUENCY 868E6
#define LORA_CS_PIN 27
#define LORA_RESET_PIN 26
#define LORA_IRQ_PIN 5
#define LORA_SCK_PIN 18
#define LORA_MISO_PIN 19
#define LORA_MOSI_PIN 23

#define FW_VERSION 1234
#define UNIX_TIME_BASE 1767225600UL // 2026-01-01 00:00:00 UTC
