/*
 * LocalNetwork_Node.ino
 *
 * Telemetry node for the LoRa Local Network protocol.
 * Reads (or simulates) temperature / humidity / battery, encodes them with
 * the 9-byte LocalPacket codec and transmits them to a gateway.
 *
 * Works on ESP32 and on plain AVR Arduinos - only the pin setup differs.
 *
 * Wiring for ESP32 + SX1276 (see docs/HARDWARE.md for the full matrix):
 *   3.3V->VCC  GND->GND  GPIO18->SCK  GPIO19->MISO  GPIO23->MOSI
 *   GPIO5->NSS  GPIO14->RST  GPIO26->DIO0
 *
 * SPDX-License-Identifier: MIT
 */

#include <SPI.h>
#include <LoRa.h>
#include <LocalPacket.h>

using namespace loralocal;

/* ----------------------------- radio configuration ------------------------ */
#define LORA_FREQ     433E6      /* 433 / 863 / 868 / 915 MHz, region specific */
#define LORA_SF       10         /* spreading factor 7..12                     */
#define LORA_BW       125E3      /* bandwidth in Hz                            */
#define LORA_TX_PWR   17         /* dBm, 2..20                                 */
#define TX_INTERVAL   10000UL    /* milliseconds between packets              */

/* ----------------------------- network identity --------------------------- */
#define NODE_ID       0x01       /* 0x01 .. 0xFE, unique per node             */
#define DEST_ID       0xFF       /* 0xFF = broadcast, or the gateway id       */

#if defined(ESP32)
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_CS     5
#define LORA_RST   14
#define LORA_IRQ   26
#endif

uint16_t packetCounter = 0;
uint8_t  batteryPct    = 96;

/* Replace these two helpers with real sensor reads (DHT22, BME280, SHT31...). */
static int16_t readTemperatureTenths() {
  return (int16_t)(284 + random(-25, 26));   /* 28.4 C +/- 2.5 C */
}

static uint16_t readHumidityTenths() {
  return (uint16_t)(720 + random(-40, 41));  /* 72.0 % +/- 4.0 % */
}

static void halt(const char* reason) {
  Serial.print(F("[ERROR] "));
  Serial.println(reason);
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("[BOOT] LocalNetwork node"));

#if defined(ESP32)
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
#endif

  if (!LoRa.begin(LORA_FREQ)) {
    halt("LoRa init failed - check wiring and power");
  }

  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setTxPower(LORA_TX_PWR);
  LoRa.enableCrc();               /* radio level integrity, 2 byte CRC */

  char msg[80];
  snprintf(msg, sizeof(msg),
           "[OK] node 0x%02X -> 0x%02X, SF%ld, %lu MHz, every %lu ms",
           (unsigned)NODE_ID, (unsigned)DEST_ID, (long)LORA_SF,
           (unsigned long)(LORA_FREQ / 1000000.0), (unsigned long)TX_INTERVAL);
  Serial.println(msg);
}

void loop() {
  Packet p;
  p.nodeId         = NODE_ID;
  p.destId         = DEST_ID;
  p.counter        = packetCounter++;
  p.temperatureX10 = readTemperatureTenths();
  p.humidityX10    = readHumidityTenths();
  p.battery        = batteryPct;

  uint8_t buf[kPacketSize];
  if (!encode(p, buf)) {
    Serial.println(F("[WARN] packet out of range, skipped"));
    delay(TX_INTERVAL);
    return;
  }

  LoRa.beginPacket();
  LoRa.write(buf, kPacketSize);
  if (LoRa.endPacket() == 0) {
    Serial.println(F("[WARN] TX failed"));
  }

  char temp[12], hum[12], line[128];
  formatTenths(temp, sizeof(temp), p.temperatureX10);
  formatTenths(hum, sizeof(hum), p.humidityX10);
  snprintf(line, sizeof(line),
           "TX cnt=%u node=0x%02X dest=0x%02X temp=%s hum=%s bat=%u len=%u",
           (unsigned)p.counter, (unsigned)p.nodeId, (unsigned)p.destId, temp, hum,
           (unsigned)p.battery, (unsigned)kPacketSize);
  Serial.println(line);

  /* slow simulated discharge so the dashboard sees a moving value */
  if (packetCounter % 60 == 0 && batteryPct > 1) batteryPct--;

  delay(TX_INTERVAL);
}
