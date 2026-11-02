#!/bin/bash
set -e

if [ "$EUID" -ne 0 ]; then
    echo "Run with sudo"
    exit 1
fi

echo "[+] Cleaning old namespaces"

for ns in ns1 ns2 ns3 ns4 ns5; do
    ip netns del "$ns" 2>/dev/null || true
done

echo "[+] Creating namespaces"

for ns in ns1 ns2 ns3 ns4 ns5; do
    ip netns add "$ns"
    ip -n "$ns" link set lo up
done

echo "[+] Creating veth pairs"

ip link add veth12 type veth peer name veth21
ip link add veth23 type veth peer name veth32
ip link add veth34 type veth peer name veth43
ip link add veth45 type veth peer name veth54

ip link set veth12 netns ns1
ip link set veth21 netns ns2

ip link set veth23 netns ns2
ip link set veth32 netns ns3

ip link set veth34 netns ns3
ip link set veth43 netns ns4

ip link set veth45 netns ns4
ip link set veth54 netns ns5

echo "[+] Configuring IP addresses"

ip -n ns1 addr add 10.0.12.1/24 dev veth12
ip -n ns2 addr add 10.0.12.2/24 dev veth21

ip -n ns2 addr add 10.0.23.2/24 dev veth23
ip -n ns3 addr add 10.0.23.3/24 dev veth32

ip -n ns3 addr add 10.0.34.3/24 dev veth34
ip -n ns4 addr add 10.0.34.4/24 dev veth43

ip -n ns4 addr add 10.0.45.4/24 dev veth45
ip -n ns5 addr add 10.0.45.5/24 dev veth54

echo "[+] Enabling interfaces"

for pair in \
    "ns1 veth12" \
    "ns2 veth21" \
    "ns2 veth23" \
    "ns3 veth32" \
    "ns3 veth34" \
    "ns4 veth43" \
    "ns4 veth45" \
    "ns5 veth54"
do
    set -- $pair
    ip -n "$1" link set "$2" up
done

echo "[+] Configuring routing"

ip -n ns2 route add default via 10.0.23.3
ip -n ns4 route add default via 10.0.34.3

echo "[+] Enabling forwarding in MITM namespace"

ip netns exec ns3 sysctl -w net.ipv4.ip_forward=1

echo "[+] Disabling reverse path filtering for the lab"

for ns in ns1 ns2 ns3 ns4 ns5; do
    ip netns exec "$ns" sysctl -qw net.ipv4.conf.all.rp_filter=0
    ip netns exec "$ns" sysctl -qw net.ipv4.conf.default.rp_filter=0
done

echo "[+] Network setup completed"

echo
echo "Namespace connectivity test:"

ip netns exec ns1 ping -c 2 10.0.12.2
ip netns exec ns2 ping -c 2 10.0.34.4
ip netns exec ns4 ping -c 2 10.0.45.5

echo
echo "All network namespaces are ready."

