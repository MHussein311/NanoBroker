#!/bin/bash

# Stop on error
set -e

echo "--- 1. Installing Python Dependencies ---"
pip install -r requirements.txt

echo "--- 2. Building & Installing NanoBroker (Python Module) ---"
# This compiles src/python_bindings.cpp and puts it in your python site-packages
pip install . 

echo "--- 3. Building C++ Producer Example ---"
mkdir -p build
cd build
cmake ..
make -j$(nproc) # Use all CPU cores

echo ""
echo "============================================"
echo "Build Complete!"
echo "To run Producer: ./build/cpp_producer"
echo "To run Consumer: python3 examples/python_consumer/main.py"
echo "============================================"