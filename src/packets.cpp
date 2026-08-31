#include "packets.h"
#include "profiles.h"
#include "config.h"
#include <string.h>
#include <time.h>

#define REGULAR_DATA_SIZE 36
#define REGULAR_PACKET_SIZE 38
#define LITE_HEADER_SIZE 17

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

static void write_i16_be(uint8_t *dst, int16_t value)
{
  dst[0] = (uint8_t)((value >> 8) & 0xFF);
  dst[1] = (uint8_t)(value & 0xFF);
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

static void prepare_nodelite_bootup(const Node *node, lora_packet *pkt)
{
  pkt->data[0] = '%';
  memcpy(&pkt->data[1], node->mac, 6);
  pkt->data[7] = 0;
  write_unix_time(&pkt->data[8], current_unix_time());
  pkt->size = 12;
  append_crc(pkt, false);
}

static void prepare_regular_data(const Node *node, lora_packet *pkt)
{
  int16_t temperature;
  int16_t humidity;
  int16_t thermocouple;
  int16_t co2;
  uint8_t sensor_type;
  uint8_t door;
  uint8_t water;
  uint8_t ble_battery;
  int8_t ble_signal;
  int8_t lora_signal;

  fill_regular_readings(node, &temperature, &humidity, &thermocouple, &co2, &sensor_type,
                        &door, &water, &ble_battery, &ble_signal, &lora_signal);

  time_t t = (time_t)current_unix_time();
  struct tm tm_buf;
  gmtime_r(&t, &tm_buf);
  uint16_t year = (uint16_t)(tm_buf.tm_year + 1900);
  uint16_t counter = node->pkt_counter;

  pkt->data[0] = 'R';
  memcpy(&pkt->data[1], node->mac, 6);
  write_i16_be(&pkt->data[7], temperature);
  write_i16_be(&pkt->data[9], humidity);
  write_i16_be(&pkt->data[11], thermocouple);
  write_i16_be(&pkt->data[13], co2);
  pkt->data[15] = sensor_type;
  pkt->data[16] = door;
  pkt->data[17] = water;
  pkt->data[18] = ble_battery;
  pkt->data[19] = (uint8_t)ble_signal;
  pkt->data[20] = (uint8_t)lora_signal;
  pkt->data[21] = 0;
  pkt->data[22] = 0;
  write_i16_be(&pkt->data[23], SENTINEL_I16);
  pkt->data[25] = 0;
  pkt->data[26] = 1;
  pkt->data[27] = (uint8_t)(counter & 0xFF);
  pkt->data[28] = (uint8_t)((counter >> 8) & 0xFF);
  pkt->data[29] = (uint8_t)tm_buf.tm_mday;
  pkt->data[30] = (uint8_t)(tm_buf.tm_mon + 1);
  pkt->data[31] = (uint8_t)(year & 0xFF);
  pkt->data[32] = (uint8_t)((year >> 8) & 0xFF);
  pkt->data[33] = (uint8_t)tm_buf.tm_hour;
  pkt->data[34] = (uint8_t)tm_buf.tm_min;
  pkt->data[35] = (uint8_t)tm_buf.tm_sec;
  pkt->size = REGULAR_DATA_SIZE;
  append_crc(pkt, true);
}

static void prepare_nodelite_data(const Node *node, lora_packet *pkt)
{
  uint16_t fv = FW_VERSION_LITE;
  uint16_t counter = node->pkt_counter;
  uint8_t sensor_buf[50];
  uint8_t total_sensor = fill_lite_sensors(node, sensor_buf, sizeof(sensor_buf));
  uint8_t sensor_bytes = (uint8_t)(total_sensor * 5);
  uint8_t header_battery = 77;

  if (node->profile == LITE_PROFILE_BATTERY)
    header_battery = (uint8_t)random(80, 101);

  pkt->data[0] = '#';
  memcpy(&pkt->data[1], node->mac, 6);
  pkt->data[7] = (uint8_t)(fv & 0xFF);
  pkt->data[8] = (uint8_t)((fv >> 8) & 0xFF);
  pkt->data[9] = (uint8_t)((counter >> 8) & 0xFF);
  pkt->data[10] = (uint8_t)(counter & 0xFF);
  pkt->data[11] = (uint8_t)random(-90, -70);
  pkt->data[12] = (uint8_t)(int8_t)random(-80, -40);
  pkt->data[13] = 0;
  pkt->data[14] = header_battery;
  pkt->data[15] = total_sensor;
  pkt->data[16] = 0;
  memcpy(&pkt->data[17], sensor_buf, sensor_bytes);

  uint8_t unix_offset = (uint8_t)(LITE_HEADER_SIZE + sensor_bytes);
  write_unix_time(&pkt->data[unix_offset], current_unix_time());
  pkt->size = (uint8_t)(unix_offset + 4);
  pkt->data[16] = pkt->size + 2;
  append_crc(pkt, false);
}

void prepare_bootup_packet(const Node *node, lora_packet *pkt)
{
  if (node->kind == NODE_KIND_REGULAR)
    prepare_regular_bootup(node, pkt);
  else
    prepare_nodelite_bootup(node, pkt);
}

void prepare_data_packet(const Node *node, lora_packet *pkt)
{
  if (node->kind == NODE_KIND_REGULAR)
    prepare_regular_data(node, pkt);
  else
    prepare_nodelite_data(node, pkt);
}
