/*
 * LocalPacket.h
 *
 * 9-byte telemetry packet codec for the LoRa Local Network protocol
 * (Node -> Gateway, local-first, no internet required).
 *
 * Wire format (big-endian, 9 bytes total):
 *
 *   offset  size  field         type    scale   range
 *   ------  ----  ------------  ------  -----   -------------------------
 *      0      1   NODE_ID       uint8    -      0x01 .. 0xFE
 *      1      1   DEST_ID       uint8    -      0x01 .. 0xFF (0xFF=bcast)
 *      2      2   COUNTER       uint16   -      0 .. 65535
 *      4      2   TEMPERATURE   int16    x10    -40.0 C .. +85.0 C
 *      6      2   HUMIDITY      uint16   x10    0.0 % .. 100.0 %
 *      8      1   BATTERY       uint8    %      0 .. 100
 *
 * Header-only, no heap, no Arduino dependency: the same file compiles on
 * AVR / ESP32 / ESP8266 sketches and on a host (g++) for unit testing.
 * Payload integrity on the air is provided by the radio CRC (LoRa.enableCrc),
 * so no checksum field is carried inside the payload.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LORALOCAL_LOCAL_PACKET_H
#define LORALOCAL_LOCAL_PACKET_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace loralocal {

static const uint8_t kPacketSize    = 9;
static const uint8_t kBroadcastId   = 0xFF;
static const uint8_t kMinNodeId     = 0x01;
static const uint8_t kMaxNodeId     = 0xFE;
static const int16_t kMinTempX10    = -400;   /* -40.0 C */
static const int16_t kMaxTempX10    =  850;   /* +85.0 C */
static const uint16_t kMaxHumX10    = 1000;   /* 100.0 % */

struct Packet {
  uint8_t  nodeId;
  uint8_t  destId;
  uint16_t counter;
  int16_t  temperatureX10;
  uint16_t humidityX10;
  uint8_t  battery;

  Packet()
      : nodeId(kMinNodeId),
        destId(kBroadcastId),
        counter(0),
        temperatureX10(0),
        humidityX10(0),
        battery(0) {}
};

enum LinkQuality {
  QualityExcellent = 0,
  QualityGood      = 1,
  QualityFair      = 2,
  QualityWeak      = 3
};

/* ------------------------------------------------------------------ helpers */

static inline bool validNodeId(uint8_t id) {
  return id >= kMinNodeId && id <= kMaxNodeId;
}

static inline void putU16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)((v >> 8) & 0xFF);
  p[1] = (uint8_t)(v & 0xFF);
}

static inline uint16_t getU16(const uint8_t* p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

/* -------------------------------------------------------------- validation */

static inline bool inRange(const Packet& p) {
  if (!validNodeId(p.nodeId)) return false;
  if (p.destId == 0x00) return false;              /* 0x00 is reserved */
  if (p.battery > 100) return false;
  if (p.humidityX10 > kMaxHumX10) return false;
  if (p.temperatureX10 < kMinTempX10) return false;
  if (p.temperatureX10 > kMaxTempX10) return false;
  return true;
}

/* ------------------------------------------------------------ encode/decode */

static inline bool encode(const Packet& pkt, uint8_t out[kPacketSize]) {
  if (out == 0) return false;
  if (!inRange(pkt)) return false;

  out[0] = pkt.nodeId;
  out[1] = pkt.destId;
  putU16(out + 2, pkt.counter);
  putU16(out + 4, (uint16_t)pkt.temperatureX10);
  putU16(out + 6, pkt.humidityX10);
  out[8] = pkt.battery;
  return true;
}

static inline bool decode(const uint8_t* in, size_t len, Packet& out) {
  if (in == 0 || len != kPacketSize) return false;

  Packet p;
  p.nodeId         = in[0];
  p.destId         = in[1];
  p.counter        = getU16(in + 2);
  p.temperatureX10 = (int16_t)getU16(in + 4);
  p.humidityX10    = getU16(in + 6);
  p.battery        = in[8];

  if (!inRange(p)) return false;
  out = p;
  return true;
}

static inline bool isFor(const Packet& pkt, uint8_t receiverId) {
  return pkt.destId == kBroadcastId || pkt.destId == receiverId;
}

/* ------------------------------------------------------------ value getters */

static inline float temperatureC(const Packet& p) {
  return (float)p.temperatureX10 / 10.0f;
}

static inline float humidityPct(const Packet& p) {
  return (float)p.humidityX10 / 10.0f;
}

/* --------------------------------------------------------- link quality map */

static inline LinkQuality linkQuality(int16_t rssi, float snr) {
  if (rssi > -90 && snr > 10) return QualityExcellent;
  if (rssi >= -100 && snr >= 5) return QualityGood;
  if (rssi >= -110 && snr >= 0) return QualityFair;
  return QualityWeak;
}

static inline const char* linkQualityName(LinkQuality q) {
  switch (q) {
    case QualityExcellent: return "Excellent";
    case QualityGood:      return "Good";
    case QualityFair:      return "Fair";
    default:               return "Weak";
  }
}

/* --------------------------------------------------------------- formatting */

/* "123" -> dst; returns bytes written (no NUL). */
static inline size_t u32ToStr(char* dst, size_t cap, uint32_t v) {
  char tmp[11];
  size_t n = 0;
  if (v == 0) {
    tmp[n++] = '0';
  } else {
    while (v > 0 && n < sizeof(tmp)) {
      tmp[n++] = (char)('0' + (v % 10));
      v /= 10;
    }
  }
  size_t out = 0;
  while (n > 0 && out + 1 < cap) dst[out++] = tmp[--n];
  dst[out] = '\0';
  return out;
}

/*
 * Format a tenths value as "28.4", "-40.0", "850.0" ...
 * Never uses printf %f (unsupported by default on AVR).
 * Returns true when the result fits into cap.
 */
static inline bool formatTenths(char* dst, size_t cap, int32_t tenths) {
  if (dst == 0 || cap < 6) return false;

  bool negative = tenths < 0;
  uint32_t v = negative ? (uint32_t)(-(int64_t)tenths) : (uint32_t)tenths;
  uint32_t whole = v / 10;
  uint32_t frac  = v % 10;

  char buf[12];
  size_t n = 0;
  if (negative) buf[n++] = '-';
  n += u32ToStr(buf + n, sizeof(buf) - n, whole);
  if (n + 2 >= sizeof(buf)) return false;
  buf[n++] = '.';
  buf[n++] = (char)('0' + frac);
  buf[n] = '\0';

  if (n + 1 > cap) return false;
  for (size_t i = 0; i <= n; i++) dst[i] = buf[i];
  return true;
}

/*
 * One-line JSON record for the gateway serial bridge, e.g.
 *   {"node":1,"dest":255,"cnt":12,"temp":28.4,"hum":72.0,"bat":84,
 *    "rssi":-95,"snr":7.5,"q":"Good"}
 * Returns true when the record fits into cap.
 */
static inline bool toJson(const Packet& p, int16_t rssi, float snr,
                          char* dst, size_t cap) {
  char temp[12], hum[12], snrBuf[12];
  int32_t snrTenths = (int32_t)(snr >= 0 ? snr * 10.0f + 0.5f : snr * 10.0f - 0.5f);
  if (!formatTenths(temp, sizeof(temp), p.temperatureX10)) return false;
  if (!formatTenths(hum, sizeof(hum), (int32_t)p.humidityX10)) return false;
  if (!formatTenths(snrBuf, sizeof(snrBuf), snrTenths)) return false;

  const char* q = linkQualityName(linkQuality(rssi, snr));
  int written = snprintf(dst, cap,
                         "{\"node\":%u,\"dest\":%u,\"cnt\":%u,\"temp\":%s,"
                         "\"hum\":%s,\"bat\":%u,\"rssi\":%d,\"snr\":%s,\"q\":\"%s\"}",
                         (unsigned)p.nodeId, (unsigned)p.destId,
                         (unsigned)p.counter, temp, hum, (unsigned)p.battery,
                         (int)rssi, snrBuf, q);
  return written > 0 && (size_t)written < cap;
}

}  /* namespace loralocal */

#endif /* LORALOCAL_LOCAL_PACKET_H */
