#include "log.h"

#include <cstdarg>
#include <cstdio>

static void write_prefix()
{
  uint32_t ms = millis();
  uint32_t hrs = ms / 3600000UL;
  ms %= 3600000UL;
  uint32_t mins = ms / 60000UL;
  ms %= 60000UL;
  uint32_t secs = ms / 1000UL;
  uint32_t msec = ms % 1000UL;

  Serial.printf("[%02u:%02u:%02u:%03u] ", hrs, mins, secs, msec);
}

void log_msg(const char *fmt, ...)
{
  char buf[256];

  write_prefix();

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  Serial.println(buf);
}

void log_msg_hex(const uint8_t *data, uint8_t len, const char *fmt, ...)
{
  char buf[256];

  write_prefix();

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  Serial.print(buf);
  for (uint8_t i = 0; i < len; i++)
    Serial.printf("%02X ", data[i]);
  Serial.println();
}
