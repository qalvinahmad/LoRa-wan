/*
 * Host unit test for src/LocalPacket.h
 *
 * Build & run (no Arduino toolchain needed):
 *   g++ -std=c++11 -Wall -Wextra -Werror -I ../../src local_packet_test.cpp -o /tmp/lp_test && /tmp/lp_test
 * or simply: ./run.sh
 *
 * SPDX-License-Identifier: MIT
 */

#include "LocalPacket.h"

#include <cstdio>
#include <cstring>

using namespace loralocal;

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    g_checks++;                                                            \
    if (!(cond)) {                                                         \
      g_fails++;                                                           \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
    }                                                                      \
  } while (0)

#define CHECK_STREQ(a, b)                                                  \
  do {                                                                     \
    g_checks++;                                                            \
    if (std::strcmp((a), (b)) != 0) {                                      \
      g_fails++;                                                           \
      std::printf("FAIL %s:%d: got \"%s\", want \"%s\"\n", __FILE__,       \
                  __LINE__, (a), (b));                                     \
    }                                                                      \
  } while (0)

static Packet sample() {
  Packet p;
  p.nodeId = 0x01;
  p.destId = 0xFF;
  p.counter = 0x0102;
  p.temperatureX10 = 284;   /* 28.4 C */
  p.humidityX10 = 720;      /* 72.0 % */
  p.battery = 84;
  return p;
}

static void test_encode_is_big_endian() {
  uint8_t buf[kPacketSize];
  Packet p = sample();
  CHECK(encode(p, buf));
  const uint8_t want[kPacketSize] = {0x01, 0xFF, 0x01, 0x02, 0x01, 0x1C, 0x02, 0xD0, 0x54};
  CHECK(std::memcmp(buf, want, kPacketSize) == 0);
}

static void test_roundtrip() {
  uint8_t buf[kPacketSize];
  Packet in = sample();
  Packet out;
  CHECK(encode(in, buf));
  CHECK(decode(buf, kPacketSize, out));
  CHECK(out.nodeId == in.nodeId);
  CHECK(out.destId == in.destId);
  CHECK(out.counter == in.counter);
  CHECK(out.temperatureX10 == in.temperatureX10);
  CHECK(out.humidityX10 == in.humidityX10);
  CHECK(out.battery == in.battery);
  CHECK(temperatureC(out) > 28.39f && temperatureC(out) < 28.41f);
  CHECK(humidityPct(out) > 71.99f && humidityPct(out) < 72.01f);
}

static void test_roundtrip_negative_temperature() {
  uint8_t buf[kPacketSize];
  Packet in = sample();
  in.temperatureX10 = -400;  /* -40.0 C, lower bound */
  Packet out;
  CHECK(encode(in, buf));
  CHECK(buf[4] == 0xFE && buf[5] == 0x70);  /* -400 as int16 big-endian */
  CHECK(decode(buf, kPacketSize, out));
  CHECK(out.temperatureX10 == -400);
}

static void test_decode_rejects_bad_length() {
  uint8_t buf[kPacketSize + 3] = {0};
  Packet out;
  CHECK(!decode(NULL, kPacketSize, out));
  CHECK(!decode(buf, 0, out));
  CHECK(!decode(buf, kPacketSize - 1, out));
  CHECK(!decode(buf, kPacketSize + 1, out));
}

static void test_encode_rejects_out_of_range() {
  uint8_t buf[kPacketSize];
  Packet p;

  p = sample(); p.nodeId = 0x00; CHECK(!encode(p, buf));
  p = sample(); p.nodeId = 0xFF; CHECK(!encode(p, buf));
  p = sample(); p.destId = 0x00; CHECK(!encode(p, buf));
  p = sample(); p.battery = 101; CHECK(!encode(p, buf));
  p = sample(); p.humidityX10 = 1001; CHECK(!encode(p, buf));
  p = sample(); p.temperatureX10 = -401; CHECK(!encode(p, buf));
  p = sample(); p.temperatureX10 = 851; CHECK(!encode(p, buf));

  /* accepted boundaries */
  p = sample(); p.nodeId = 0x01; CHECK(encode(p, buf));
  p = sample(); p.nodeId = 0xFE; CHECK(encode(p, buf));
  p = sample(); p.destId = 0xFF; CHECK(encode(p, buf));
  p = sample(); p.battery = 100; CHECK(encode(p, buf));
  p = sample(); p.humidityX10 = 1000; CHECK(encode(p, buf));
  p = sample(); p.temperatureX10 = 850; CHECK(encode(p, buf));
}

static void test_decode_rejects_corrupt_ranges() {
  uint8_t buf[kPacketSize];
  Packet p = sample();
  Packet out;
  CHECK(encode(p, buf));

  buf[0] = 0x00; CHECK(!decode(buf, kPacketSize, out));
  CHECK(encode(p, buf)); buf[0] = 0xFF; CHECK(!decode(buf, kPacketSize, out));
  CHECK(encode(p, buf)); buf[1] = 0x00; CHECK(!decode(buf, kPacketSize, out));
  CHECK(encode(p, buf)); buf[8] = 101; CHECK(!decode(buf, kPacketSize, out));
}

static void test_destination_filter() {
  Packet p = sample();
  CHECK(isFor(p, 0x42));            /* broadcast reaches everyone */
  p.destId = 0x42;
  CHECK(isFor(p, 0x42));
  CHECK(!isFor(p, 0x43));
}

static void test_link_quality_table() {
  CHECK(linkQuality(-80, 12.0f) == QualityExcellent);
  CHECK(linkQuality(-95, 7.0f) == QualityGood);
  CHECK(linkQuality(-105, 3.0f) == QualityFair);
  CHECK(linkQuality(-115, -2.0f) == QualityWeak);
  CHECK(linkQuality(-95, 4.0f) == QualityFair);   /* SNR below Good band */
  CHECK(linkQuality(-101, 9.0f) == QualityFair);  /* RSSI below Good band */
  CHECK_STREQ(linkQualityName(QualityExcellent), "Excellent");
  CHECK_STREQ(linkQualityName(QualityGood), "Good");
  CHECK_STREQ(linkQualityName(QualityFair), "Fair");
  CHECK_STREQ(linkQualityName(QualityWeak), "Weak");
}

static void test_format_tenths() {
  char buf[16];
  CHECK(formatTenths(buf, sizeof(buf), 284));  CHECK_STREQ(buf, "28.4");
  CHECK(formatTenths(buf, sizeof(buf), -400)); CHECK_STREQ(buf, "-40.0");
  CHECK(formatTenths(buf, sizeof(buf), 850));  CHECK_STREQ(buf, "85.0");
  CHECK(formatTenths(buf, sizeof(buf), 0));    CHECK_STREQ(buf, "0.0");
  CHECK(formatTenths(buf, sizeof(buf), 5));    CHECK_STREQ(buf, "0.5");
  CHECK(formatTenths(buf, sizeof(buf), -5));   CHECK_STREQ(buf, "-0.5");
  CHECK(!formatTenths(buf, 5, 284));           /* too small */
}

static void test_to_json() {
  char line[192];
  Packet p = sample();
  CHECK(toJson(p, -95, 7.5f, line, sizeof(line)));
  CHECK(std::strstr(line, "\"node\":1") != NULL);
  CHECK(std::strstr(line, "\"dest\":255") != NULL);
  CHECK(std::strstr(line, "\"cnt\":258") != NULL);   /* 0x0102 */
  CHECK(std::strstr(line, "\"temp\":28.4") != NULL);
  CHECK(std::strstr(line, "\"hum\":72.0") != NULL);
  CHECK(std::strstr(line, "\"bat\":84") != NULL);
  CHECK(std::strstr(line, "\"rssi\":-95") != NULL);
  CHECK(std::strstr(line, "\"snr\":7.5") != NULL);
  CHECK(std::strstr(line, "\"q\":\"Good\"") != NULL);

  char small[16];
  CHECK(!toJson(p, -95, 7.5f, small, sizeof(small)));  /* does not fit */
}

static void test_u32_to_str() {
  char buf[12];
  CHECK(u32ToStr(buf, sizeof(buf), 0) == 1);  CHECK_STREQ(buf, "0");
  CHECK(u32ToStr(buf, sizeof(buf), 7) == 1);  CHECK_STREQ(buf, "7");
  CHECK(u32ToStr(buf, sizeof(buf), 65535) == 5); CHECK_STREQ(buf, "65535");
  CHECK(u32ToStr(buf, 1, 42) == 0);           /* cap=1 -> truncated to "" */
  CHECK_STREQ(buf, "");
}

int main() {
  test_encode_is_big_endian();
  test_roundtrip();
  test_roundtrip_negative_temperature();
  test_decode_rejects_bad_length();
  test_encode_rejects_out_of_range();
  test_decode_rejects_corrupt_ranges();
  test_destination_filter();
  test_link_quality_table();
  test_format_tenths();
  test_to_json();
  test_u32_to_str();

  std::printf("%d checks, %d failures\n", g_checks, g_fails);
  return g_fails == 0 ? 0 : 1;
}
