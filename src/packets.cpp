#include "packets.h"
#include "config.h"
#include <string.h>
#include <time.h>

// Same algorithm as Zephyr crc16_itu_t() — CRC-16/XMODEM / CCITT.
// poly 0x1021, init 0x0000, xorOut 0x0000, refin/refout false
static uint16_t crc16_itu_t(uint16_t seed, const uint8_t *src, size_t len)
{
  for (; len > 0; len--)
  {
    seed = (uint16_t)((seed >> 8) | (seed << 8));
    seed ^= *src++;
    seed ^= (uint16_t)((seed & 0xff) >> 4);
    seed ^= (uint16_t)(seed << 12);
    seed ^= (uint16_t)((seed & 0xff) << 5);
  }
  return seed;
}

static void append_crc(lora_packet *pkt, bool without_command)
{
  size_t offset = without_command ? 1 : 0;
  uint16_t crc = crc16_itu_t(0, &pkt->data[offset], pkt->size - offset);
  pkt->data[pkt->size] = (uint8_t)crc;
  pkt->data[pkt->size + 1] = (uint8_t)(crc >> 8);
  pkt->size += 2;
}

static void write_unix_time(uint8_t *dst, uint32_t unix_time)
{
  dst[0] = (uint8_t)unix_time;
  dst[1] = (uint8_t)(unix_time >> 8);
  dst[2] = (uint8_t)(unix_time >> 16);
  dst[3] = (uint8_t)(unix_time >> 24);
}

static void prepare_nodelite_bootup(const Node *node, lora_packet *pkt)
{
  pkt->data[0] = '%';
  memcpy(&pkt->data[1], node->mac, 6);
  pkt->data[7] = 0;
  write_unix_time(&pkt->data[8], current_unix_time());
  pkt->size = 12;
  append_crc(pkt, false);
}

static void prepare_regular_bootup(const Node *node, lora_packet *pkt)
{
  pkt->data[0] = 'A';
  memcpy(&pkt->data[1], node->mac, 6);
  pkt->data[7] = 0;
  write_unix_time(&pkt->data[8], current_unix_time());
  pkt->size = 12;
  append_crc(pkt, false);
}

#pragma pack(push, 1)
struct sensor_data
{
  int16_t mantissa;
  int8_t exponent;
  uint8_t enum_value;
  uint8_t enum_type;
};
#pragma pack(pop)

static void prepare_nodelite_data(const Node *node, lora_packet *pkt)
{
  uint16_t fv = FW_VERSION;
  uint16_t counter = node->pkt_counter;
  int16_t mantissa = (int16_t)(-800 + (node->id - 1));

  pkt->data[0] = '#';
  memcpy(&pkt->data[1], node->mac, 6);
  pkt->data[7] = (uint8_t)fv;
  pkt->data[8] = (uint8_t)(fv >> 8);
  pkt->data[9] = (uint8_t)counter;
  pkt->data[10] = (uint8_t)(counter >> 8);
  pkt->data[11] = 10;
  pkt->data[12] = (uint8_t)(-50);
  pkt->data[13] = 0;
  pkt->data[14] = 77;
  pkt->data[15] = 1;
  pkt->data[16] = 28;

  sensor_data sensor;
  sensor.mantissa = mantissa;
  sensor.exponent = -1;
  sensor.enum_value = 1;
  sensor.enum_type = 4;
  memcpy(&pkt->data[17], &sensor, sizeof(sensor));
  write_unix_time(&pkt->data[22], current_unix_time());
  pkt->size = 26;
  append_crc(pkt, false);
}

static void prepare_regular_data(const Node *node, lora_packet *pkt)
{
  int16_t offset = (int16_t)((node->id - 1) * 10);
  int16_t temperature = (int16_t)(2150 + offset);
  int16_t humidity = (int16_t)(3150 + offset);
  int16_t thermocouple = (int16_t)(1150 + offset);
  int16_t co2 = (int16_t)(1234 + (node->id - 1));
  uint16_t fv = FW_VERSION;
  uint16_t counter = node->pkt_counter;
  time_t t = (time_t)current_unix_time();
  struct tm tm_buf;
  gmtime_r(&t, &tm_buf);
  uint16_t year = (uint16_t)(tm_buf.tm_year + 1900);

  pkt->data[0] = 'R';
  memcpy(&pkt->data[1], node->mac, 6);
  pkt->data[7] = (uint8_t)temperature;
  pkt->data[8] = (uint8_t)(temperature >> 8);
  pkt->data[9] = (uint8_t)humidity;
  pkt->data[10] = (uint8_t)(humidity >> 8);
  pkt->data[11] = (uint8_t)thermocouple;
  pkt->data[12] = (uint8_t)(thermocouple >> 8);
  pkt->data[13] = (uint8_t)co2;
  pkt->data[14] = (uint8_t)(co2 >> 8);
  pkt->data[15] = 1;
  pkt->data[16] = 35;
  pkt->data[17] = 70;
  pkt->data[18] = 10;
  pkt->data[19] = (uint8_t)(-50);
  pkt->data[20] = 0;
  pkt->data[21] = 25;
  pkt->data[22] = 55;
  pkt->data[23] = (uint8_t)fv;
  pkt->data[24] = (uint8_t)(fv >> 8);
  pkt->data[25] = (uint8_t)counter;
  pkt->data[26] = (uint8_t)(counter >> 8);
  pkt->data[27] = (uint8_t)tm_buf.tm_mday;
  pkt->data[28] = (uint8_t)(tm_buf.tm_mon + 1);
  pkt->data[29] = (uint8_t)year;
  pkt->data[30] = (uint8_t)(year >> 8);
  pkt->data[31] = (uint8_t)tm_buf.tm_hour;
  pkt->data[32] = (uint8_t)tm_buf.tm_min;
  pkt->data[33] = (uint8_t)tm_buf.tm_sec;
  pkt->size = 34;
  append_crc(pkt, true);
}

void prepare_bootup_packet(const Node *node, lora_packet *pkt)
{
#if NODE_VARIANT == 0
  prepare_regular_bootup(node, pkt);
#else
  prepare_nodelite_bootup(node, pkt);
#endif
}

void prepare_data_packet(const Node *node, lora_packet *pkt)
{
#if NODE_VARIANT == 0
  prepare_regular_data(node, pkt);
#else
  prepare_nodelite_data(node, pkt);
#endif
}
