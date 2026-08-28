#include <Arduino.h>
#include "node.h"
#include "lora_radio.h"
#include "emulator.h"

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;

  nodes_init();

  if (!radio_init())
  {
    Serial.println("LoRa init failed. Check your connections.");
    while (true)
      ;
  }

  emulator_start();
}

void loop()
{
  emulator_tick();
}
