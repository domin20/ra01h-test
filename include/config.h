#pragma once

#define NODE_COUNT 50
#define NODE_COUNT_MAX 50

#define BOOT_INTERVAL_MS 60000UL
/* After Regular boot TX: wait up to this long for gateway ACK before next node. */
#define BOOT_ACK_WAIT_REGULAR_MS 3000UL
#define COMM_INTERVAL_MS 600000UL
#define TX_GAP_MS 1000UL
#define DATA_ACK_RETRY_MS 1000UL
#define DATA_ACK_MAX_RETRIES 3U

#define NODE_SLEEP_TIME_SEC 600U
#define NODE_TIME_SLOT_SEC 10U

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

#define FW_VERSION_REGULAR 100   // encoded as whole + fraction / 100 -> 1.00
#define FW_VERSION_LITE 1000     // divided by 1000 on gateway -> 1.000
#define UNIX_TIME_BASE 1767225600UL
