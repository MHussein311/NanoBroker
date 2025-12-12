# Performance Benchmarks

All benchmarks were conducted on **WSL 2 (Ubuntu 24.04)**.

## 1. Throughput

| Metric | Result | Notes |
| :--- | :--- | :--- |
| Payload | 2.76 MB | 1280x720 RGB |
| Transfer Time | < 40 µs | |
| FPS | ~10,000+ | |
| Bandwidth | ~27 GB/s | Zero copy |

## 2. Latency Comparison

| Method | 8B | 2.7MB |
| :--- | :--- | :--- |
| UDS | 25.4 µs | 200-400 µs |
| NanoBroker | 0.08 µs | 0.04 µs |

## 3. CPU Efficiency
Hybrid spin → yield → sleep strategy.

## 4. Methodology
Producer C++, Consumer Python, measure latency and throughput.
