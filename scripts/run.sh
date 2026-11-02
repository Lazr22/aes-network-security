#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if [ "$EUID" -ne 0 ]; then
    echo "Run with sudo"
    exit 1
fi

if [ ! -f "$ROOT/key.hex" ]; then
    echo "AES key missing. Generate key.hex first."
    exit 1
fi

KEY=$(cat "$ROOT/key.hex")

if [ "${#KEY}" -ne 64 ]; then
    echo "Invalid AES key length"
    exit 1
fi

if [ ! -f "$ROOT/files/test_128mb.bin" ]; then
    echo "Test file missing"
    exit 1
fi

echo "[+] Starting five terminals"

# Terminal 5: Receiver

xterm -T "5 - Receiver" \
-e bash -lc "
ip netns exec ns5 '$ROOT/build/receiver' \
5000 '$ROOT/files/received.bin';
echo Receiver stopped;
exec bash
" &

sleep 1

# Terminal 4: Device 2

xterm -T "4 - Device 2 - DECRYPT" \
-e bash -lc "
ip netns exec ns4 '$ROOT/build/device' \
decrypt 4001 10.0.45.5 5000 '$KEY';
echo Device 2 stopped;
exec bash
" &

sleep 1

# Terminal 2: Device 1

xterm -T "2 - Device 1 - ENCRYPT" \
-e bash -lc "
ip netns exec ns2 '$ROOT/build/device' \
encrypt 4000 10.0.34.4 4001 '$KEY';
echo Device 1 stopped;
exec bash
" &

sleep 1

# Terminal 3: Passive MITM monitoring

xterm -T "3 - MITM - MONITOR" \
-e bash -lc "
ip netns exec ns3 tcpdump \
-ni any -s 96 -nn -X 'tcp port 4001';
exec bash
" &

sleep 2

# Terminal 1: Sender

xterm -T "1 - Sender" \
-e bash -lc "
ip netns exec ns1 '$ROOT/build/sender' \
10.0.12.2 4000 '$ROOT/files/test_128mb.bin';
echo Sender stopped;
exec bash
" &

echo "[+] All five terminals launched"
echo "[+] Observe the transfer in each terminal"

