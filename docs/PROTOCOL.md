# Local Network Protocol

A deliberately small telemetry frame for local-first LoRa links: 9 bytes,
fixed, no parsing ambiguity. The codec that implements everything below lives
in [`src/LocalPacket.h`](../src/LocalPacket.h).

## Design goals

| Goal | Decision |
| ---- | -------- |
| Short airtime | fixed 9-byte payload, no variable fields, no headers inside the payload |
| Trivial decoding | one `decode()` call, big-endian, no bitfield packing tricks |
| Fail loudly | every out-of-range value is rejected at decode time instead of silently accepted |
| Zero dependencies | plain C++ header — compiles on AVR/ESP32 and on a desktop for tests |
| Integrity | handled by the radio CRC (`LoRa.enableCrc()`), not by another checksum in the payload |

## Frame layout

```text
  Byte  │ Size │ Field       │ Type  │ Scale │ Range              │ Example
  ------┼──────┼─────────────┼───────┼───────┼────────────────────┼─────────────
    0   │  1   │ NODE_ID     │ u8    │  -    │ 0x01 .. 0xFE       │ 0x01
    1   │  1   │ DEST_ID     │ u8    │  -    │ 0x01 .. 0xFF       │ 0xFF = bcast
    2   │  2   │ COUNTER     │ u16   │  -    │ 0 .. 65535         │ 0x0102 = #258
    4   │  2   │ TEMPERATURE │ i16   │ ×10   │ -400 .. 850        │ 0x011C = 28.4 °C
    6   │  2   │ HUMIDITY    │ u16   │ ×10   │ 0 .. 1000          │ 0x02D0 = 72.0 %
    8   │  1   │ BATTERY     │ u8    │  %    │ 0 .. 100           │ 0x54 = 84 %
  ------┴──────┴─────────────┴───────┴───────┴────────────────────┴─────────────
  Total: 9 bytes  +  LoRa PHY header  +  radio CRC (2 bytes, appended by the radio)
```

* **Endianness:** every multi-byte field is **big-endian** (`COUNTER 0x0102`
  is transmitted as `0x01, 0x02`).
* **Scales:** temperature and humidity are stored as integer *tenths* so the
  whole frame stays integral — no float on the wire, no precision drift, and
  the same bytes can be printed on an 8-bit AVR without `printf("%f")`.
* **Ranges:** `-40.0 … +85.0 °C` matches the common SHT/DHT/BME sensor class;
  clamp your sensor readings before `encode()` or the packet is rejected.

## Validation rules

`decode()` returns `false` (and leaves the output untouched) unless **all** of
these hold:

| Check | Reason |
| ----- | ------ |
| length is exactly 9 | anything else is not this protocol |
| `NODE_ID` in `0x01..0xFE` | `0x00` unused, `0xFF` reserved for broadcast |
| `DEST_ID != 0x00` | `0x00` reserved as "invalid address" |
| `BATTERY <= 100` | percentage above 100 means a corrupt frame |
| `HUMIDITY <= 1000` | above 100.0 % is impossible |
| `-400 <= TEMPERATURE <= 850` | outside the sensor class range |

`encode()` applies the same rules, so a misconfigured node never puts an
impossible frame on the air.

## Addressing

```text
0xFF  broadcast      every receiver accepts it (default for sensor nodes)
0x01..0xFE  unicast  only the device with that id accepts it
0x00  reserved       rejected on encode and decode
```

Receivers must filter with `isFor(packet, myId)`:

```cpp
if (!decode(buf, len, p))            { /* corrupt */ return; }
if (!isFor(p, GATEWAY_ID))           { /* for someone else */ return; }
```

Convention used by the examples: node `0x01`, gateway `0x10`. Change them
freely as long as every id in the network is unique.

## Counter semantics

* Starts at `0` for each boot, increments per transmitted frame.
* Wraps `65535 → 0` — treat it as a *sequence hint*, not as a security token.
* Use it to drop duplicates (re-transmissions of the same frame) and to spot
  gaps: `expected = last + 1` tells you how many frames were lost.

## Why no checksum inside the payload

LoRa already appends a 2-byte CRC over the whole frame when
`LoRa.enableCrc()` is enabled; a corrupted frame never reaches `decode()`.
A second checksum would cost bytes on every packet to defend against errors
the radio has already filtered. Field-range validation (table above) catches
the *semantic* corruption case — a valid CRC over nonsense values — which is
the failure mode a checksum would not catch anyway.

## Air time

Symbol time is `2^SF / BW`. With the defaults of the examples
(SF10, 125 kHz, 8-symbol preamble, 9-byte payload) one frame is roughly:

| Spreading factor | Symbol time | One frame ≈  |
| ---------------- | ----------- | ------------ |
| SF7              | 1.0 ms      | ~15 ms       |
| SF9              | 4.1 ms      | ~60 ms       |
| SF10             | 8.2 ms      | ~115 ms      |
| SF12             | 32.8 ms     | ~460 ms      |

Higher SF = longer range and more robustness, but slower and more power hungry.
SF7–SF9 is usually enough for 1 km line of sight; SF10–SF12 for the 3–5 km
cases described in the README.

## Security notes

The payload travels in **cleartext** — anyone with a SDR and the same
frequency/BW/SF can read it, and there is no authentication, so a spoofed node
is possible. For a local sensor network this is a documented trade-off;
payload encryption is on the roadmap. Do not use this protocol for anything
that controls locks, money or safety systems.

## The serial bridge format (gateway → host)

The gateway prints one JSON object per line (115200 baud, newline delimited):

```json
{"node":1,"dest":255,"cnt":12,"temp":28.4,"hum":72.0,"bat":84,"rssi":-95,"snr":7.5,"q":"Good"}
{"bad":true,"len":4,"rssi":-101,"snr":32}
{"stat":true,"rx":120,"ok":118,"bad":1,"wrongdest":1}
```

| Field   | Type    | Meaning                                            |
| ------- | ------- | -------------------------------------------------- |
| `node`  | int     | sender id                                          |
| `dest`  | int     | destination id (`255` = broadcast)                 |
| `cnt`   | int     | frame counter from the sender                      |
| `temp`  | number  | °C, one decimal                                    |
| `hum`   | number  | % RH, one decimal                                  |
| `bat`   | int     | battery %                                          |
| `rssi`  | int     | dBm, per-packet                                     |
| `snr`   | number  | dB, one decimal                                     |
| `q`     | string  | `Excellent` / `Good` / `Fair` / `Weak` (see table) |
| `bad`   | bool    | frame failed validation; `len` = received length   |
| `stat`  | bool    | periodic counters line                             |

Link-quality bands (RSSI **and** SNR must both qualify):

| RSSI (dBm)  | SNR (dB) | `q`         |
| ----------- | -------- | ----------- |
| > −90       | > 10     | Excellent   |
| −90 … −100  | 5 … 10   | Good        |
| −100 … −110 | 0 … 5    | Fair        |
| < −110      | < 0      | Weak        |

## Extending the protocol

Need an extra field (soil moisture, GPS, alarm flag)?

1. **Append, never insert** — put the new field after `BATTERY` so old
   decoders still read the first 9 bytes correctly.
2. **Bump a version byte** if the change is not backward compatible: reserve
   the first nibble of `NODE_ID`, or — cleaner — negotiate the format out of
   band (a provisioning frame) and keep `decode()` strict.
3. **Keep it integral** — tenths/integers, scale in the decoder. Floats on the
   wire cost formatting code on AVR and break determinism.
4. **Add tests first** — extend `extras/tests/local_packet_test.cpp`; the
   suite runs in CI on every push.

## Related code

| Piece | Where |
| ----- | ----- |
| Codec (encode/decode/validate/JSON) | `src/LocalPacket.h` |
| Host unit tests | `extras/tests/local_packet_test.cpp` |
| Node example | `examples/LocalNetwork_Node` |
| Gateway example | `examples/LocalNetwork_Gateway` |
| Sensor-field mapping guide | [GETTING_STARTED.md](GETTING_STARTED.md) |
