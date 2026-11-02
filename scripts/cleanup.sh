#!/bin/bash

if [ "$EUID" -ne 0 ]; then
    echo "Run with sudo"
    exit 1
fi

for ns in ns1 ns2 ns3 ns4 ns5; do
    ip netns del "$ns" 2>/dev/null || true
done

echo "All namespaces removed."

