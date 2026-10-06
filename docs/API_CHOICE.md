# Which API should I use?

This repository ships three radio stacks side by side on purpose: the same
hardware (SX127x/SX126x breakout on an ESP32) can be driven by two different
APIs, and a third one appears only when you need real LoRaWAN. This page is
the decision table.

## TL;DR

| You are building… | Use | Example to copy |
| ----------------- | --- | --------------- |
| A local sensor network (node ↔ gateway, own protocol) | `<LoRa.h>` (arduino-LoRa) | `examples/LocalNetwork_Node`, `examples/LocalNetwork_Gateway` |
| Anything with SX126x / SX1280 / LR11x0 / CC1101 / RF69 / nRF24 | `<RadioLib.h>` | `examples/SX127x/*`, `examples/SX126x/*` |
| A LoRaWAN device for TTN / ChirpStack | `<RadioLib.h>` LoRaWAN | `examples/LoRaWAN/LoRaWAN_Starter` |
| Reading the official MAC source / writing a bare-metal port | LoRaMac-node | `extras/mac/` (reference only) |

## 1. `LoRa` (arduino-LoRa) — the simple one

```cpp
#include <LoRa.h>
LoRa.begin(433E6);
LoRa.beginPacket(); LoRa.write(buf, 9); LoRa.endPacket();
```

* **Strengths:** smallest API surface, easiest to read, perfect for a fixed
  9-byte telemetry frame, plenty of tutorials.
* **Limits:** SX127x family only, blocking transmit by default, no LoRaWAN,
  no FSK/other modulations.
* **Best for:** the LocalNetwork examples, teaching, first prototypes.

## 2. `RadioLib` — the complete one

```cpp
#include <RadioLib.h>
SX1276 radio = new Module(5, 26, 14);
radio.begin(433.0);
radio.transmit(payload, len);
```

* **Strengths:** one consistent API across **SX127x, SX126x, SX1280, LR11x0,
  CC1101, RF69, nRF24, Si443x**; non-blocking transmit/receive with
  interrupts; radio settings as first-class values (bandwidth in Hz, spreading
  factor as enum); **LoRaWAN stack included**; AX.25, RTTY, SSTV, Morse and
  more protocols on top.
* **Cost:** more classes to learn (`Module`, `PhysicalLayer`, module object)
  and noticeably more flash — on an ATmega328P (Uno) heavy sketches exceed
  32 kB, so prefer ESP32/Mega/STM32 for RadioLib examples.
* **Best for:** production nodes, multi-radio products, anything that will
  grow beyond one modulation.

### Porting a `LoRa` sketch to RadioLib

```cpp
// arduino-LoRa                          // RadioLib
LoRa.setPins(CS, RST, IRQ);       Module m = new Module(CS, IRQ, RST);
LoRa.begin(433E6);                SX1276 radio = &m; radio.begin(433.0);
LoRa.setSpreadingFactor(10);       radio.setSpreadingFactor(10);
LoRa.setSignalBandwidth(125E3);    radio.setBandwidth(125.0);
LoRa.setTxPower(17);               radio.setTxPower(17);
LoRa.enableCrc();                  // CRC on by default
LoRa.beginPacket(); ... endPacket();  radio.transmit(buf, len);
LoRa.parsePacket();                radio.startReceive();
```

## 3. LoRaMac-node — the standard stack

The official LoRaWAN Class A/B/C MAC implementation by Semtech/STACKFORCE is
included under [`extras/mac/`](../extras/mac/) as **reference source** — it is
never compiled into your sketch (it needs the LoRaMac-node radio driver and a
board HAL, which this repository does not bundle).

* **Read it when** you want to understand join/ADR/class-B/C behaviour down
  to the byte.
* **Port it when** you are building a product on an STM32/CC1352 target and
  want the official MAC — see `extras/mac/README.md` for what the port needs.
* **For Arduino today** use RadioLib's LoRaWAN instead:
  `examples/LoRaWAN/LoRaWAN_Starter` (+ `notes.md` explains TTN key pasting).

## How the repository is laid out

```text
src/                  RadioLib + LoRa + LocalPacket.h  (compiled)
  RadioLib.h            every RadioLib module and protocol
  LoRa.h / LoRa.cpp     classic arduino-LoRa API
  LocalPacket.h         9-byte LocalNetwork codec (original, unit-tested)
examples/             147 sketches: RadioLib (by topic) + arduino-LoRa (root)
  LocalNetwork_Node/    original node example (protocol)
  LocalNetwork_Gateway/ original gateway example (JSON bridge)
extras/
  mac/                  LoRaMac-node MAC, reference only, not compiled
  licenses/             upstream license texts
  tests/                host unit tests (g++), run in CI
docs/                 this documentation
```

## Compatibility notes

* One install provides **both** `<LoRa.h>` and `<RadioLib.h>` — you can migrate
  file by file.
* They do not conflict at link time (different symbol sets), but do **not**
  drive the same radio from both APIs at once: pick one `begin()` owner.
* RadioLib examples are per-topic folders (`examples/SX127x/…`); the classic
  `LoRa*` sketches at the example root come from arduino-LoRa.
