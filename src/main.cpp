#include <Arduino.h>
#include "emulator.h"
#include "log.h"
#include "lora_radio.h"
#include "node.h"

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;

  time_init();
  nodes_init();

  if (!radio_init())
  {
    LOG("LoRa init failed. Check your connections.");
    while (true)
      ;
  }

  emulator_start();
}

void loop()
{
  emulator_tick();
}
