#pragma once

#include <stdint.h>

struct Node;

// NodeLite sensor / unit enums (gateway radio_lora_protocol_processor.c)
#define SENSOR_TYPE_BATTERY 1
#define SENSOR_TYPE_SHT 4
#define SENSOR_TYPE_THERMOCOUPLE 5
#define SENSOR_TYPE_BMP 6
#define SENSOR_TYPE_VAISALA 7
#define SENSOR_TYPE_LIGHT 8
#define SENSOR_TYPE_CO2 10

#define UNIT_DEGREES 1
#define UNIT_PERCENT 2
#define UNIT_PASCAL 3
#define UNIT_LUX 8

#define REGULAR_NODE_COUNT 20
#define LITE_NODE_COUNT 30
#define SENTINEL_I16 -25500 // -255.0 after /100 scaling

enum NodeKind : uint8_t
{
  NODE_KIND_REGULAR = 0,
  NODE_KIND_LITE = 1,
};

enum RegularProfile : uint8_t
{
  REG_PROFILE_VAISALA = 0, // MAC 01-03, m=1
  REG_PROFILE_LN2 = 1,     // MAC 04-05, LN2 level + temp
  REG_PROFILE_KTYPE = 2,   // MAC 06-10, m=2
  REG_PROFILE_BLE = 3,     // MAC 11-15, m=3
  REG_PROFILE_BLE_IO = 4,  // MAC 16-20, m=3 + door/water
};

enum LiteProfile : uint8_t
{
  LITE_PROFILE_ENV = 0,        // MAC 21-25
  LITE_PROFILE_MULTI_TEMP = 1, // MAC 26-30
  LITE_PROFILE_AIR = 2,        // MAC 31-35
  LITE_PROFILE_BATTERY = 3,    // MAC 36-40
  LITE_PROFILE_TC_ONLY = 4,    // MAC 41-45
  LITE_PROFILE_LIGHT_ONLY = 5, // MAC 46-50
};

void node_apply_profile(Node *node);
const char *node_kind_name(NodeKind kind);
const char *regular_profile_name(RegularProfile profile);
const char *lite_profile_name(LiteProfile profile);

void fill_regular_readings(const Node *node, int16_t *temperature, int16_t *humidity,
                           int16_t *thermocouple, int16_t *co2, uint8_t *sensor_type,
                           uint8_t *door, uint8_t *water, uint8_t *ble_battery,
                           int8_t *ble_signal, int8_t *lora_signal, uint8_t *ln2_percent,
                           int16_t *ln2_sensor);

uint8_t fill_lite_sensors(const Node *node, uint8_t *dst, uint8_t max_bytes);
