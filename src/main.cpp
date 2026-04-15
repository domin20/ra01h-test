#include <Arduino.h>
#include <SPI.h> // include libraries
#include <LoRa.h>

const long frequency = 868E6; // LoRa Frequency

const int csPin = 27;    // LoRa radio chip select
const int resetPin = 26; // LoRa radio reset
const int irqPin = 5;    // change for your board; must be a hardware interrupt pin

struct lora_packet
{
  uint8_t data[100];
  uint8_t size;
};

lora_packet packet;

#define IS_TX_MODULE 0

void prepareNodeLiteBootUpPacket();
void prepareRegularNodeBootUpPacket();
void prepareNodeLitePacket();
void prepareRegularNodePacket();

void LoRa_rxMode();
void LoRa_txMode();
void LoRa_sendPacket(lora_packet *pkt);
void LoRa_sendMessage(String message);
void onReceive(int packetSize);
void onTxDone();
boolean runEvery(unsigned long interval);

void setup()
{
  Serial.begin(115200); // initialize serial
  while (!Serial)
    ;

  SPI.begin(18, 19, 23, csPin);
  LoRa.setPins(csPin, resetPin, irqPin);

  if (!LoRa.begin(frequency))
  {
    Serial.println("LoRa init failed. Check your connections.");
    while (true)
      ; // if failed, do nothing
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5); // CR 4/5

  Serial.println("LoRa init succeeded.");

#if IS_TX_MODULE == 0
  LoRa.onReceive(onReceive);
  LoRa_rxMode();
#else
  LoRa.onTxDone(onTxDone);
#endif
}

void loop()
{
  // #if IS_TX_MODULE == 1
  prepareNodeLiteBootUpPacket();
  LoRa_sendPacket(&packet);
  LoRa_rxMode();
  delay(1000);

  prepareRegularNodeBootUpPacket();
  LoRa_sendPacket(&packet);
  LoRa_rxMode();
  delay(1000);

  prepareNodeLitePacket();
  LoRa_sendPacket(&packet);
  LoRa_rxMode();
  delay(1000);

  prepareRegularNodePacket();
  LoRa_sendPacket(&packet);
  LoRa_rxMode();
  delay(1000);

  Serial.println("Send Message!");
  // #endif
}

void prepareNodeLiteBootUpPacket()
{
  uint32_t unix_time = 1767225600UL;

  packet.data[0] = '%';
  packet.data[1] = 'A';
  packet.data[2] = 'B';
  packet.data[3] = 'C';
  packet.data[4] = 'D';
  packet.data[5] = 'E';
  packet.data[6] = 'F';
  packet.data[7] = 0;
  packet.data[8] = unix_time;
  packet.data[9] = unix_time >> 8;
  packet.data[10] = unix_time >> 16;
  packet.data[11] = unix_time >> 24;
  uint16_t crc16 = 0x1123;
  packet.data[12] = crc16;
  packet.data[13] = crc16 >> 8;
  packet.size = 14;
}

void prepareRegularNodeBootUpPacket()
{
  prepareNodeLiteBootUpPacket();
  packet.data[0] = 'A';
  uint16_t crc16 = 26757;
  packet.data[12] = crc16;
  packet.data[13] = crc16 >> 8;
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

void prepareNodeLitePacket()
{
  uint32_t unix_time = 1767225600UL;

  packet.data[0] = '#';
  packet.data[1] = 'A';
  packet.data[2] = 'B';
  packet.data[3] = 'C';
  packet.data[4] = 'D';
  packet.data[5] = 'E';
  packet.data[6] = 'F';
  uint16_t fv = 1234;
  uint16_t pckCounter = 123;
  packet.data[7] = fv;
  packet.data[8] = fv >> 8;
  packet.data[9] = pckCounter;
  packet.data[10] = pckCounter >> 8;
  packet.data[11] = 10;  // ble signal
  packet.data[12] = -50; // lora signal
  packet.data[13] = 0;   // user
  packet.data[14] = 77;  // battery
  packet.data[15] = 1;   // total sensor
  packet.data[16] = 28;  // packet size

  sensor_data sensor;
  sensor.mantissa = -800;
  sensor.exponent = -1;
  sensor.enum_value = 1;
  sensor.enum_type = 4;

  memcpy(&packet.data[17], &sensor, sizeof(sensor));
  packet.data[22] = unix_time;
  packet.data[23] = unix_time >> 8;
  packet.data[24] = unix_time >> 16;
  packet.data[25] = unix_time >> 24;
  uint16_t crc16 = 23666;
  packet.data[26] = crc16;
  packet.data[27] = crc16 >> 8;
  packet.size = 28;
}

void prepareRegularNodePacket()
{
  packet.data[0] = 'R';
  packet.data[1] = 'A';
  packet.data[2] = 'B';
  packet.data[3] = 'C';
  packet.data[4] = 'D';
  packet.data[5] = 'E';
  packet.data[6] = 'F';
  int16_t temperature = 2150;
  int16_t humidity = 3150;
  int16_t thermocouple = 1150;
  int16_t co2 = 1234;
  packet.data[7] = temperature;
  packet.data[8] = temperature >> 8;
  packet.data[9] = humidity;
  packet.data[10] = humidity >> 8;
  packet.data[11] = thermocouple;
  packet.data[12] = thermocouple >> 8;
  packet.data[13] = co2;
  packet.data[14] = co2 >> 8;
  packet.data[15] = 1;   // sensor type
  packet.data[16] = 35;  // proximity sensor
  packet.data[17] = 70;  // ble battery
  packet.data[18] = 10;  // ble signal
  packet.data[19] = -50; // lora signal
  packet.data[20] = 0;   // user
  packet.data[21] = 25;  // ln2
  packet.data[22] = 55;  // ln2 sensor
  uint16_t fv = 1234;
  packet.data[23] = fv;
  packet.data[24] = fv >> 8;

  uint16_t pckCounter = 123;
  packet.data[25] = pckCounter;
  packet.data[26] = pckCounter >> 8;

  uint16_t year = 2026;
  packet.data[27] = 13;        // day
  packet.data[28] = 4;         // month
  packet.data[29] = year;      // year
  packet.data[30] = year >> 8; // year
  packet.data[31] = 14;        // hours
  packet.data[32] = 48;        // minutes
  packet.data[33] = 15;        // seconds

  uint16_t crc16 = 45003;
  packet.data[34] = crc16;
  packet.data[35] = crc16 >> 8;
  packet.size = 36;
}

void LoRa_rxMode()
{
  LoRa.disableInvertIQ(); // normal mode
  // LoRa.enableInvertIQ(); // active invert I and Q signals
  LoRa.receive(); // set receive mode
}

void LoRa_txMode()
{
  LoRa.idle();            // set standby mode
  LoRa.disableInvertIQ(); // normal mode
}

void LoRa_sendPacket(lora_packet *pkt)
{
  LoRa_txMode();                    // set tx mode
  LoRa.beginPacket();               // start packet
  LoRa.write(pkt->data, pkt->size); // add payload
  LoRa.endPacket(false);            // finish packet and send it
}

void LoRa_sendMessage(String message)
{
  LoRa_txMode();         // set tx mode
  LoRa.beginPacket();    // start packet
  LoRa.print(message);   // add payload
  LoRa.endPacket(false); // finish packet and send it
}

void onReceive(int packetSize)
{
  Serial.print("Node Receive: ");
  while (LoRa.available())
  {
    Serial.print(LoRa.read());
    Serial.print(" ");
  }
  Serial.println();
}

void onTxDone()
{
  Serial.println("TxDone");
  LoRa_rxMode();
}

boolean runEvery(unsigned long interval)
{
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}
