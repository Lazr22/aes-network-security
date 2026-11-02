AES Network Security Project

A Linux networking laboratory implementing TCP file transfer with AES-256-GCM encryption and authentication using five Linux network namespaces.

Architecture
text
NS1             NS2              NS3             NS4             NS5
Sender  --->    Device 1  --->   MITM  --->     Device 2  --->  Receiver
                Encrypt          Monitor        Decrypt
                                 

Technologies
C
Linux Network Namespaces
Virtual Ethernet (veth)
TCP/IP
OpenSSL
AES-256-GCM
SHA-256
Bash
Features
Five isolated network namespaces.
TCP-based file transfer.
AES-256-GCM encryption and authentication.
Passive MITM traffic monitoring.
Authentication tag verification.
SHA-256 file integrity verification.
Automated terminal launching.
Installation
bash
sudo apt update
sudo apt install build-essential libssl-dev iproute2 tcpdump xterm openssl

Build
bash
make

Setup
bash
openssl rand -hex 32 > key.hex
chmod 600 key.hex

dd if=/dev/urandom of=files/test_128mb.bin bs=1M count=128

sudo ./scripts/setup.sh

Run
bash
sudo ./scripts/run.sh

Verify
bash
sha256sum files/test_128mb.bin
sha256sum files/received.bin


Both hashes should match.

Cleanup
bash
sudo ./scripts/cleanup.sh

Security

This is an educational networking project.

The initial implementation uses a pre-shared AES key. It is not a production-ready secure communication protocol.
