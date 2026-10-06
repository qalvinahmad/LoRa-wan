# Third-party licenses

This repository bundles code from three upstream projects. Each keeps its own
license; the full texts are stored under [`extras/licenses/`](extras/licenses/).

| Component | Upstream | License | Bundled as |
| --- | --- | --- | --- |
| **RadioLib** — radio drivers (SX127x, SX126x, SX128x, LR11x0, CC1101, RF69, nRF24, …) and protocols (LoRaWAN, AX.25, RTTY, SSTV, ADS-B, …) by Jan Gromeš | <https://github.com/jgromes/RadioLib> | MIT | `src/` (except `LoRa.*`, `API.md`), `examples/` (all except the `LoRa*` sketches at the root) |
| **arduino-LoRa** — simple `LoRa` API for SX127x by Sandeep Mistry | <https://github.com/sandeepmistry/arduino-LoRa> | MIT | `src/LoRa.h`, `src/LoRa.cpp`, `src/API.md`, `examples/LoRa*` sketches at the example root |
| **LoRaMac-node** — Semtech/STACKFORCE LoRaWAN MAC (Class A/B/C) | <https://github.com/Lora-net/LoRaMac-node> | Revised BSD | `extras/mac/` — **reference only, never compiled** |

License texts:

- [`extras/licenses/RadioLib-license.txt`](extras/licenses/RadioLib-license.txt)
- [`extras/licenses/arduino-LoRa-LICENSE`](extras/licenses/arduino-LoRa-LICENSE)
- [`extras/licenses/LoRaMac-node-LICENSE`](extras/licenses/LoRaMac-node-LICENSE)

The bundled files retain their original copyright headers. This repository's own
glue (library metadata, documentation, this file) is MIT — see [`LICENSE`](LICENSE).
