#pragma once

#include <Arduino.h>

void log_msg(const char *fmt, ...);
void log_msg_hex(const uint8_t *data, uint8_t len, const char *fmt, ...);

#define LOG(...) log_msg(__VA_ARGS__)
