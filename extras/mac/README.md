# `extras/mac/` — LoRaMac-node MAC (referensi, tidak di-compile)

Salinan **lengkap** dari [`LoRaMac-node/src/mac`](https://github.com/Lora-net/LoRaMac-node) (Semtech / STACKFORCE, Revised BSD) — MAC layer LoRaWAN Class A/B/C beserta semua region (`EU868`, `US915`, `AU915`, `AS923`, `CN470`, `CN779`, `EU433`, `IN865`, `KR920`, `RU864`), crypto, parser/serializer, dan MAC commands.

## Kenapa ada di `extras/`, bukan `src/`

Arduino meng-compile **semua** `.c`/`.cpp` di dalam `src/` untuk setiap sketch.
`LoRaMac.c` butuh dependensi yang sengaja tidak ikut dibundel di repo ini:

- `radio.h` + driver radio (`src/radio/`) — antarmuka radio Semtech, bukan RadioLib
- `utilities.h`, `timer.h`, `systime.h`, `delay.h` (`src/system/`) — semuanya butuh *board HAL*
- `boards/` (`spi-board.h`, `gpio-board.h`, `timer-board.h`, …) — khusus board STM32/LoRaMac-node

Folder `extras/` secara resmi **diabaikan oleh Arduino IDE** (lihat spesifikasi layout library Arduino), jadi kode referensi ini tidak pernah masuk proses build dan tidak merusak sketch Anda.

## Kalau benar-benar mau menjalankannya

Butuh port, bukan salinan:

1. Salin `src/radio/` + `src/system/` dari LoRaMac-node.
2. Tulis board HAL untuk target Anda (mis. ESP32: `gpio-board`, `spi-board`, `timer-board`, `delay-board`, `systime`).
3. Sediakan `SX1276-board.h`/`.c` (mapping pin) — konfigurasi pin mengikuti wiring di `readme.md`.

Untuk LoRaWAN di Arduino tanpa port tersebut, gunakan jalur yang sudah jalan:
[`RadioLib` LoRaWAN](../../src/protocols/LoRaWAN/) — lihat `examples/LoRaWAN/`
(`LoRaWAN_Starter`, `LoRaWAN_Reference`, `LoRaWAN_ABP`, `LoRaWAN_Class_C`,
`LoRaWAN_Multicast`, `LoRaWAN_Package_Manager`).

## Lisensi

Revised BSD — teks lengkap di [`../licenses/LoRaMac-node-LICENSE`](../licenses/LoRaMac-node-LICENSE).
File-file di folder ini disalin apa adanya dari upstream (`LoRaMac-node-master`)
dan mempertahankan header copyright aslinya.
