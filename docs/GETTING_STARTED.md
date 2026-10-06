# Getting Started

End-to-end path from zero to a packet arriving from a node into a JSON line on
your computer.

## 1. Install the library

**Option A — Arduino IDE (recommended for a first run)**

1. Download this repository as a ZIP (`Code → Download ZIP`).
2. `Sketch → Include Library → Add .ZIP Library…`
3. Restart the IDE. Both APIs are now available:
   `#include <LoRa.h>` and `#include <RadioLib.h>`.

**Option B — copy the folder**

```text
~/Arduino/libraries/LoRa-wan/     <- this repository
```

**Option C — PlatformIO**

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    symlink://../LoRa-wan      ; or a git URL once published
```

Why one library instead of three? See [API_CHOICE.md](API_CHOICE.md).

## 2. Wire the radio

ESP32 + SX1276 (the default in every LocalNetwork example):

```text
ESP32          SX1276
3.3V    --->   VCC
GND     --->   GND
GPIO18  --->   SCK
GPIO19  --->   MISO
GPIO23  --->   MOSI
GPIO5   --->   NSS (CS)
GPIO14  --->   RST
GPIO26  --->   DIO0 (IRQ)
```

More boards and modules: [HARDWARE.md](HARDWARE.md).

> Never power the radio from 5 V. Antenna first: a missing antenna can damage
> the PA within seconds of the first transmit.

## 3. Set your region frequency

```cpp
#define LORA_FREQ 433E6   // 433 / 863 / 868 / 915 MHz - pick ONE, legal in your region
```

Node and gateway **must use the same frequency, spreading factor and
bandwidth** or they will not see each other at all.

## 4. Upload the pair

| Side     | Sketch                                    | Serial output               |
| -------- | ----------------------------------------- | --------------------------- |
| Node     | `examples/LocalNetwork_Node`              | `TX cnt=0 node=0x01 ...`    |
| Gateway  | `examples/LocalNetwork_Gateway`           | one JSON line per packet    |

Set `NODE_ID` in the node and `GATEWAY_ID` in the gateway (both `0x01..0xFE`,
never `0xFF` which is reserved for broadcast).

Open the gateway serial monitor at **115200 baud**. You should see:

```json
{"node":1,"dest":255,"cnt":0,"temp":28.4,"hum":72.0,"bat":96,"rssi":-72,"snr":9.5,"q":"Excellent"}
```

That JSON line is the contract between the radio and everything above it —
forward it straight into a database, a chart, an MQTT topic or a websocket.

## 5. Understand the packet

```text
 byte 0      byte 1      bytes 2-3    bytes 4-5    bytes 6-7   byte 8
 NODE_ID     DEST_ID     COUNTER      TEMPERATURE  HUMIDITY    BATTERY
 (u8)        (u8)        (u16 BE)     (i16 BE x10) (u16 BE x10)(u8 %)
```

The codec lives in one header: [`src/LocalPacket.h`](../src/LocalPacket.h).
Full spec, validation rules and extension guidance: [PROTOCOL.md](PROTOCOL.md).

## 6. Validate without hardware

```sh
extras/tests/run.sh
# 86 checks, 0 failures
```

The packet codec is host-testable C++: no board, no IDE, just `g++`.

## Next steps

- Swap the simulated sensor values in the node for a real sensor
  (DHT22 / BME280 / SHT31 — the two helpers are `readTemperatureTenths()` and
  `readHumidityTenths()`).
- Restrict the gateway to unicast: set `DEST_ID` to the gateway id in the node.
- Need LoRaWAN (TTN / ChirpStack) instead of a local network? Use the bundled
  RadioLib examples in `examples/LoRaWAN/` — see [API_CHOICE.md](API_CHOICE.md).
- Something not working? [TROUBLESHOOTING.md](TROUBLESHOOTING.md).
