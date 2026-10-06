# Troubleshooting

## Compile-time

### `'LoRa.h': No such file or directory`
The library is not installed where the IDE/PlatformIO looks.
* Arduino IDE: `Sketch → Include Library → Add .ZIP Library…`, restart the IDE.
* PlatformIO: make sure `lib_deps` points at this folder (`symlink://…`) and
  the path is correct relative to `platformio.ini`.

### `Sketch uses 33938 bytes (105%) ... text section exceeds available space`
RadioLib sketches (and the LoRaWAN stack) are too big for an ATmega328P
(Uno/Nano, 32 kB flash). The code is fine — the board is not.
* Use an **ESP32**, Mega, STM32 or any board with ≥128 kB flash, or
* stick to the `LoRa*` sketches at the example root (arduino-LoRa), which fit
  in 8–12 kB, e.g. `LocalNetwork_Node` → 7.3 kB on an Uno.

### `unable to find numeric literal operator 'operator""x'` in `examples/LoRaWAN/...`
Expected: those examples ship with **placeholders** (`0x--`) where your TTN /
ChirpStack keys belong. Follow `notes.md` next to the sketch and paste your
DevEUI / AppKey / NwkKey. The sketch does not compile until they are real hex
values — that is upstream RadioLib's design, not a broken copy.

### `'STM32WLx' does not name a type` on ESP32 / AVR
`examples/STM32WLx/*` targets the STM32WL SoC (integrated radio, pins like
`PC3`). It fails on every other board, upstream RadioLib included. Use
`examples/SX127x/*` or `examples/SX126x/*` instead.

### `'SPI' was not declared` / undefined reference after moving files
Keep the repository as one Arduino library (`library.properties` at the root,
sources under `src/`). Copying only `examples/` out of the tree breaks the
relative includes.

## Runtime

### `LoRa init failed - check wiring and power`
In order of likelihood:

1. **NSS/CS pin wrong** — the radio never answers; on ESP32 the default in the
   examples is GPIO5, on Uno D10.
2. **SPI pins not initialised on ESP32** — `SPI.begin(SCK, MISO, MOSI, CS)`
   and `LoRa.setPins(CS, RST, IRQ)` must run **before** `LoRa.begin()`.
3. **Power** — 3.3 V only, and the rail must survive ~120 mA TX peaks
   (see [HARDWARE.md](HARDWARE.md#power)).
4. **Dead module or wrong SPI bus** (VSPI/HSPI conflict with another device).

### Gateway stays silent although the node transmits
* Same `LORA_FREQ`, `LORA_SF` and `LORA_BW` on **both** ends — including the
  MHz decimal (`433E6` vs `868E6` is the classic mistake).
* Both use `LoRa.enableCrc()` (the examples do).
* Gateway id ≠ node id; broadcast (`0xFF`) is accepted by everyone, so a
  silent gateway usually means a radio/config mismatch, not an address issue.
* Serial monitor at **115200** — lower baud shows unreadable lines.

### I get frames but they are rejected (`{"bad":true,...}`)
`decode()` rejects anything outside the protocol ranges. Print the raw bytes:

```cpp
Serial.print(len); for (int i = 0; i < len; i++) { Serial.print(buf[i], HEX); }
```

* `len != 9` → you are mixing two protocols or a fragment arrived.
* Values out of range → the sender is not using `LocalPacket.h`, or a field
  (e.g. humidity > 100 %) was not clamped before `encode()`.

### Random `bad` frames / low link quality
Look at `q` in the JSON line:

| `q` | Action |
| --- | ------ |
| Excellent / Good | nothing to do |
| Fair | raise the antenna, reduce SF, shorten the cable to the antenna |
| Weak | increase distance testing before raising TX power; check antenna band, add clearance from metal, consider a 1/4 wave antenna tuned to your frequency |

RSSI around −110 dBm or SNR below 0 dB is the edge of decodability: reduce
spreading factor (faster, less processing gain) or improve placement.

### Range is far below the advertised 1–5 km
* SF12 ≠ magic: obstacles and 2.4 GHz noise dominate.
* Antenna type and height matter more than `setTxPower()`.
* Duty cycle limits may force you to longer intervals (EU 868: 1 %).

### Both `LoRa` and `RadioLib` are included — is that a problem?
Including both headers in one sketch is fine (this repository links them
together for every build). **Driving one radio from both APIs at the same
time** is not: let exactly one of them call `begin()` on a given module.

## Tests / CI

### `extras/tests/run.sh` fails locally
Needs a host C++ compiler (`g++` or `clang++`) and nothing else:

```sh
g++ --version || xcode-select --install    # macOS
extras/tests/run.sh
```

The suite prints `N checks, 0 failures` on success and exits non-zero on any
failed check.

### CI is red on GitHub Actions
Open the failing job: the two common cases are (a) the unit test — reproduce
locally with `extras/tests/run.sh`, and (b) the Arduino compile job —
reproduce with

```sh
arduino-cli compile --fqbn arduino:avr:uno --library . examples/LocalNetwork_Node
```

Still stuck? Open an issue with the exact error text, your board, the radio
module and the wiring diagram.
