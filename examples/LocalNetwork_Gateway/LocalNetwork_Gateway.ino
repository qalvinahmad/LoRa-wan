/*
 * LocalNetwork_Gateway.ino
 *
 * Gateway receiver for the LoRa Local Network protocol.
 * Every valid frame is forwarded as one JSON line on Serial (115200), so any
 * host bridge - USB, ESP32 WiFi TCP, MQTT publisher, serial-to-database -
 * can consume it without parsing radio specifics.
 *
 * Output examples:
 *   {"node":1,"dest":255,"cnt":12,"temp":28.4,"hum":72.0,"bat":84,
 *    "rssi":-95,"snr":7.5,"q":"Good"}
 *   {"bad":true,"len":4,"rssi":-101,"snr":3.2}
 *   {"stat":true,"rx":120,"ok":118,"bad":1,"wrongdest":1}
 *
 * Works on ESP32 and plain AVR Arduinos - only the pin setup differs.
 * SPDX-License-Identifier: MIT
 */

#include <SPI.h>
#include <LoRa.h>
#include <LocalPacket.h>

using namespace loralocal;

/* ----------------------------- radio configuration ------------------------ */
#define LORA_FREQ     433E6      /* must match the nodes                     */
#define LORA_SF       10
#define LORA_BW       125E3
#define LORA_RX_GAIN  0          /* 0 = default (AGC)                        */

/* ----------------------------- network identity --------------------------- */
#define GATEWAY_ID    0x10       /* 0x01 .. 0xFE, unique per gateway         */
#define STAT_EVERY    20         /* print a stats line every N good frames   */

#if defined(ESP32)
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_CS     5
#define LORA_RST   14
#define LORA_IRQ   26
#endif

uint32_t statRx = 0;
uint32_t statOk = 0;
uint32_t statBad = 0;
uint32_t statWrongDest = 0;

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
  Serial.println(F("[BOOT] LocalNetwork gateway"));

#if defined(ESP32)
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
#endif

  if (!LoRa.begin(LORA_FREQ)) {
    halt("LoRa init failed - check wiring and power");
  }

  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.enableCrc();

  char msg[72];
  snprintf(msg, sizeof(msg), "[OK] gateway 0x%02X listening, SF%ld, %lu MHz",
           (unsigned)GATEWAY_ID, (long)LORA_SF,
           (unsigned long)(LORA_FREQ / 1000000.0));
  Serial.println(msg);
}

static void printStats() {
  char line[128];
  snprintf(line, sizeof(line),
           "{\"stat\":true,\"rx\":%lu,\"ok\":%lu,\"bad\":%lu,\"wrongdest\":%lu}",
           (unsigned long)statRx, (unsigned long)statOk,
           (unsigned long)statBad, (unsigned long)statWrongDest);
  Serial.println(line);
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize == 0) {
    return;
  }

  statRx++;

  uint8_t buf[kPacketSize + 8];
  size_t len = 0;
  while (LoRa.available() && len < sizeof(buf)) {
    buf[len++] = (uint8_t)LoRa.read();
  }

  int16_t rssi = (int16_t)LoRa.packetRssi();
  float snr = LoRa.packetSnr();

  Packet p;
  if (!decode(buf, len, p)) {
    statBad++;
    char line[96];
    snprintf(line, sizeof(line), "{\"bad\":true,\"len\":%u,\"rssi\":%d,\"snr\":%ld}",
             (unsigned)len, (int)rssi, (long)(snr * 10));
    Serial.println(line);
    return;
  }

  if (!isFor(p, GATEWAY_ID)) {
    statWrongDest++;
    return;                       /* frame for another gateway / node */
  }

  char line[192];
  if (toJson(p, rssi, snr, line, sizeof(line))) {
    Serial.println(line);
    statOk++;
  } else {
    statBad++;
    Serial.println(F("{\"bad\":true,\"reason\":\"overflow\"}"));
  }

  if (statOk > 0 && statOk % STAT_EVERY == 0) {
    printStats();
  }
}
