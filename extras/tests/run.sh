#!/bin/sh
# Build and run the host unit tests for src/LocalPacket.h (no Arduino needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/lora_local_tests"
g++ -std=c++11 -Wall -Wextra -Werror -I ../../src local_packet_test.cpp -o "$OUT"
exec "$OUT"
