#!/bin/bash
echo "Installing System Dependencies (Requires Sudo)..."
sudo apt update
sudo apt install -y build-essential cmake g++ \
    python3-dev python3-pip \
    libopencv-dev python3-opencv
echo "Done."