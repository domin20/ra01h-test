#include "profiles.h"
#include "node.h"
#include <math.h>

static int16_t rand_i16(int16_t lo, int16_t hi)
{
  return (int16_t)random(lo, hi + 1);
}

static int16_t float_to_i16(float value)
{
  return (int16_t)lroundf(value * 100.0f);
}

static void encode_sensor(uint8_t *dst, float value, uint8_t unit, uint8_t type)
{
  int16_t mantissa;
  int8_t exponent = -2;
  float scaled = value * 100.0f;

  /* mantissa is int16 — values above ~327.67 with exp -2 overflow and decode negative */
  while (fabsf(scaled) > 32767.0f && exponent < 10) {
    exponent++;
    scaled = value * powf(10.0f, (float)(-exponent));
  }

  mantissa = (int16_t)lroundf(scaled);

  dst[0] = (uint8_t)(mantissa & 0xFF);
  dst[1] = (uint8_t)((mantissa >> 8) & 0xFF);
  dst[2] = (uint8_t)exponent;
  dst[3] = unit;
  dst[4] = type;
}

void node_apply_profile(Node *node)
{
  if (node->id <= REGULAR_NODE_COUNT)
  {
    node->kind = NODE_KIND_REGULAR;
    if (node->id <= 3)
      node->profile = REG_PROFILE_VAISALA;
    else if (node->id <= 5)
      node->profile = REG_PROFILE_LN2;
    else if (node->id <= 10)
      node->profile = REG_PROFILE_KTYPE;
    else if (node->id <= 15)
      node->profile = REG_PROFILE_BLE;
    else
    {
      node->profile = REG_PROFILE_BLE_IO;
      switch (node->id)
      {
      case 16:
        node->door = 0;
        node->water = 2;
        break;
      case 17:
        node->door = 1;
        node->water = 2;
        break;
      case 18:
        node->door = 2;
        node->water = 1;
        break;
      case 19:
        node->door = 2;
        node->water = 0;
        break;
      default: // 20
        node->door = 0;
        node->water = 1;
        break;
      }
    }
    return;
  }

  node->kind = NODE_KIND_LITE;
  node->door = 2;
  node->water = 2;

  uint8_t lite_index = node->id - REGULAR_NODE_COUNT; // 1..30
  if (lite_index <= 5)
    node->profile = LITE_PROFILE_ENV;
  else if (lite_index <= 10)
    node->profile = LITE_PROFILE_MULTI_TEMP;
  else if (lite_index <= 15)
    node->profile = LITE_PROFILE_AIR;
  else if (lite_index <= 20)
    node->profile = LITE_PROFILE_BATTERY;
  else if (lite_index <= 25)
    node->profile = LITE_PROFILE_TC_ONLY;
  else
    node->profile = LITE_PROFILE_LIGHT_ONLY;
}

const char *node_kind_name(NodeKind kind)
{
  return kind == NODE_KIND_REGULAR ? "Regular" : "NodeLite";
}

const char *regular_profile_name(RegularProfile profile)
{
  switch (profile)
  {
  case REG_PROFILE_VAISALA:
    return "Vaisala";
  case REG_PROFILE_LN2:
    return "LN2";
  case REG_PROFILE_KTYPE:
    return "Ktype";
  case REG_PROFILE_BLE:
    return "BLE";
  case REG_PROFILE_BLE_IO:
    return "BLE+IO";
  default:
    return "?";
  }
}

const char *lite_profile_name(LiteProfile profile)
{
  switch (profile)
  {
  case LITE_PROFILE_ENV:
    return "Env";
  case LITE_PROFILE_MULTI_TEMP:
    return "MultiTemp";
  case LITE_PROFILE_AIR:
    return "Air";
  case LITE_PROFILE_BATTERY:
    return "Battery";
  case LITE_PROFILE_TC_ONLY:
    return "TC";
  case LITE_PROFILE_LIGHT_ONLY:
    return "Light";
  default:
    return "?";
  }
}

void fill_regular_readings(const Node *node, int16_t *temperature, int16_t *humidity,
                           int16_t *thermocouple, int16_t *co2, uint8_t *sensor_type,
                           uint8_t *door, uint8_t *water, uint8_t *ble_battery,
                           int8_t *ble_signal, int8_t *lora_signal, uint8_t *ln2_percent,
                           int16_t *ln2_sensor)
{
  *temperature = SENTINEL_I16;
  *humidity = SENTINEL_I16;
  *thermocouple = SENTINEL_I16;
  *co2 = SENTINEL_I16;
  *ble_battery = 255;
  *ble_signal = -80;
  *lora_signal = (int8_t)rand_i16(-80, -40);
  *door = 2;
  *water = 2;
  *ln2_percent = 255;
  *ln2_sensor = SENTINEL_I16;

  switch ((RegularProfile)node->profile)
  {
  case REG_PROFILE_VAISALA:
    *sensor_type = 1;
    *temperature = float_to_i16(random(2200, 2501) / 100.0f);
    *humidity = float_to_i16(random(4000, 4501) / 100.0f);
    *co2 = float_to_i16(random(5, 16) / 100.0f);
    *ble_battery = 100;
    break;

  case REG_PROFILE_LN2:
    *sensor_type = 4;
    *ln2_percent = (uint8_t)random(35, 96);
    *ln2_sensor = float_to_i16(random(-19600, -17800) / 100.0f);
    break;

  case REG_PROFILE_KTYPE:
    *sensor_type = 2;
    *temperature = float_to_i16(random(2000, 3001) / 100.0f);
    *thermocouple = *temperature;
    break;

  case REG_PROFILE_BLE:
    *sensor_type = 3;
    *temperature = float_to_i16(random(2200, 2501) / 100.0f);
    *humidity = float_to_i16(random(4000, 4501) / 100.0f);
    *ble_battery = (uint8_t)random(80, 101);
    *ble_signal = (int8_t)rand_i16(-90, -70);
    break;

  case REG_PROFILE_BLE_IO:
    *sensor_type = 3;
    *temperature = float_to_i16(random(2200, 2501) / 100.0f);
    *humidity = float_to_i16(random(4000, 4501) / 100.0f);
    *ble_battery = (uint8_t)random(80, 101);
    *ble_signal = (int8_t)rand_i16(-90, -70);
    *door = node->door;
    *water = node->water;
    break;

  default:
    *sensor_type = 0;
    break;
  }
}

uint8_t fill_lite_sensors(const Node *node, uint8_t *dst, uint8_t max_bytes)
{
  uint8_t offset = 0;
  auto append = [&](float value, uint8_t unit, uint8_t type) -> bool
  {
    if (offset + 5 > max_bytes)
      return false;
    encode_sensor(&dst[offset], value, unit, type);
    offset += 5;
    return true;
  };

  switch ((LiteProfile)node->profile)
  {
  case LITE_PROFILE_ENV:
    append(random(2200, 2501) / 100.0f, UNIT_DEGREES, SENSOR_TYPE_SHT);
    append(random(4000, 4501) / 100.0f, UNIT_PERCENT, SENSOR_TYPE_SHT);
    append((float)random(101000, 102001), UNIT_PASCAL, SENSOR_TYPE_BMP);
    break;

  case LITE_PROFILE_MULTI_TEMP:
    append(random(2200, 2501) / 100.0f, UNIT_DEGREES, SENSOR_TYPE_SHT);
    append(random(2000, 3001) / 100.0f, UNIT_DEGREES, SENSOR_TYPE_THERMOCOUPLE);
    append(random(2200, 2501) / 100.0f, UNIT_DEGREES, SENSOR_TYPE_VAISALA);
    break;

  case LITE_PROFILE_AIR:
    append(random(2200, 2501) / 100.0f, UNIT_DEGREES, SENSOR_TYPE_SHT);
    append(random(4000, 4501) / 100.0f, UNIT_PERCENT, SENSOR_TYPE_SHT);
    append(random(5, 16) / 100.0f, UNIT_PERCENT, SENSOR_TYPE_CO2);
    append((float)random(200, 801), UNIT_LUX, SENSOR_TYPE_LIGHT);
    break;

  case LITE_PROFILE_BATTERY:
    append((float)random(80, 101), UNIT_PERCENT, SENSOR_TYPE_BATTERY);
    append(random(2200, 2501) / 100.0f, UNIT_DEGREES, SENSOR_TYPE_SHT);
    break;

  case LITE_PROFILE_TC_ONLY:
    append(random(2000, 3001) / 100.0f, UNIT_DEGREES, SENSOR_TYPE_THERMOCOUPLE);
    break;

  case LITE_PROFILE_LIGHT_ONLY:
    append((float)random(100, 1001), UNIT_LUX, SENSOR_TYPE_LIGHT);
    break;

  default:
    break;
  }

  return offset / 5;
}
