# Hardware

## Modules

| Module | Band | Driver in this repo | Notes |
| ------ | ---- | ------------------- | ----- |
| SX1276 / SX1278 | 433 / 868 / 915 MHz | arduino-LoRa **and** RadioLib | the default in every LocalNetwork example |
| SX1262 / SX1268 | 150–960 MHz | RadioLib | lower current TX, needs `DIO1` instead of `DIO0` |
| SX1280 | 2.4 GHz | RadioLib | short range, no license needed in most regions |
| LR11x0 | sub-GHz + 2.4 GHz | RadioLib | GNSS/Wi-Fi scan capable |
| CC1101 | sub-GHz FSK | RadioLib | no LoRa — FSK/OOK only |
| RF69 / Si443x / nRF24 | sub-GHz / 2.4 GHz | RadioLib | short range alternatives |

Check the silkscreen of your module: `SX1276` and `SX1278` differ mainly in
the matching network, not in the register map, so both work with the same code.

## Wiring

### ESP32 dev board + SX127x (default of the examples)

```text
ESP32            SX127x
3.3V       --->  VCC
GND        --->  GND
GPIO18     --->  SCK
GPIO19     --->  MISO
GPIO23     --->  MOSI
GPIO5      --->  NSS (CS)
GPIO14     --->  RST
GPIO26     --->  DIO0 (IRQ)
```

### Arduino Uno / Nano + SX127x

```text
Arduino            SX127x
3.3V        --->   VCC        (not 5V - the radio is 3.3V only)
GND         --->   GND
D13 (SCK)   --->   SCK
D12 (MISO)  --->   MISO
D11 (MOSI)  --->   MOSI
D10         --->   NSS (CS)
D9          --->   RST
D2          --->   DIO0 (IRQ)
```

On AVR the examples call `LoRa.begin()` without `SPI.begin(sck, miso, mosi, ss)`
because the hardware SPI pins are fixed — that is the only difference from the
ESP32 path (`#if defined(ESP32)` in both sketches).

### ESP8266 (Wemos D1 mini)

```text
ESP8266           SX127x
3.3V        --->  VCC
GND         --->  GND
D5 (GPIO14) --->  SCK
D6 (GPIO12) --->  MISO
D7 (GPIO13) --->  MOSI
D8 (GPIO15) --->  NSS (CS)
D3 (GPIO0)  --->  RST
D2 (GPIO4)  --->  DIO0 (IRQ)
```

### SX126x modules

Replace `DIO0` with `DIO1` (interrupt pin) and keep NSS/RST. With RadioLib:

```cpp
SX1262 radio = new Module(5, 26, 14);   // NSS, IRQ(DIO1), RESET
```

## Power

* Supply the radio from **3.3 V**. On most dev boards the on-board 3.3 V
  regulator can source 500 mA+, which is enough for TX peaks.
* Do **not** feed VCC from 5 V (Uno 5 V pin) — the SX127x is not 5 V tolerant.
* Long TX bursts at 17–20 dBm draw >120 mA peaks; a weak 3.3 V rail shows up
  as `LoRa init failed` or random CRC errors.
* For battery nodes: the radio idle current is a few µA in sleep
  (`LoRa.sleep()`), TX dominates the budget — see the power-profiling item in
  the README roadmap.

## Antenna & range

* Use an antenna tuned for your band (433 / 868 / 915 MHz). A wrong-band
  antenna reflects power back into the PA and can destroy it.
* **Transmit with an antenna connected.** This is the single most common way
  to kill a LoRa module.
* Antenna placement matters more than TX power: 1–2 m above the ground plane,
  away from metal enclosures.
* Range expectations (line of sight vs built-up) are documented in the
  README's *Range* section: roughly **1–5 km** depending on SF, antenna and
  terrain.

## Frequencies and legal limits

| Region | Common band | Example define |
| ------ | ----------- | -------------- |
| Europe | 863–865 / 868–870 MHz | `868E6` |
| North America | 902–928 MHz | `915E6` |
| China | 470–510 MHz, 433–435 MHz | `433E6` |
| Other Asia | varies by country | check your regulator |

Band plans and power limits change: verify the current allocation with your
national regulator (for example SDPPI/Kominfo in Indonesia, ETSI in Europe,
FCC in the US) before deploying anything permanent.

Duty-cycle rules (e.g. 1 % in the EU 868 MHz band) limit how often you may
transmit: at 1 % duty cycle a 100 ms frame may be sent at most once every
10 s — which is exactly what `TX_INTERVAL 10000` in the node example does.

## Checklist before blaming the code

1. Antenna connected.
2. VCC = 3.3 V, GND common between MCU and radio.
3. Same frequency / spreading factor / bandwidth on both ends.
4. `SPI.begin(...)` + `LoRa.setPins(...)` called **before** `LoRa.begin()`
   (ESP32 only).
5. `LoRa.begin()` returned `true`; if not, see
   [TROUBLESHOOTING.md](TROUBLESHOOTING.md).
