#!/bin/bash

# Create necessary directories
mkdir -p iso/boot/grub

# Build the kernel
make all

# Create ISO
make iso

echo "ISO image created successfully!" 