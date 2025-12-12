# NanoBroker Architecture

NanoBroker is a low-latency IPC system built on **POSIX Shared Memory** and **C++20 Atomics**. Unlike socket-based IPC, which relies on the OS kernel to route data, NanoBroker maps a single block of physical RAM into the virtual address space of multiple independent processes.

## 1. System Overview

The system follows a **Broadcast Ring Buffer** topology.

```mermaid
+-------------------------------------------------------+
|                 Shared Memory Segment                 |
|  (/dev/shm/video_stream)                              |
+-------------------------------------------------------+
| [ Header ]                                            |
|   - Head Index (Atomic)                               |
|   - Tail Indices [0..15] (Atomic)                     |
|   - Slot Active Flags (Atomic)                        |
|   - Write Lock (Atomic Flag)                          |
+-------------------------------------------------------+
| [ Data Buffer ]                                       |
|   [ Slot 0: 6MB Pixel Buffer ]                        |
|   [ Slot 1: 6MB Pixel Buffer ]                        |
|   ...                                                 |
|   [ Slot N: 6MB Pixel Buffer ]                        |
+-------------------------------------------------------+
```

### 1.1 Zero-Copy Transport
Traditional IPC (Sockets/Pipes): Data is copied from User Space A → Kernel Buffer → User Space B.
NanoBroker: Process A writes to address 0x7f.... Process B reads from address 0x7f.... The data never leaves RAM, and the CPU never performs a memcpy.

## 2. Synchronization Model

NanoBroker uses a Lock-Free design for readers and a Spinlock for writers.

### 2.1 The Producer (Writer)
- Acquire spinlock
- Check backpressure
- Write data
- Commit update to head

### 2.2 The Consumer (Reader)
- Check head
- Compare tail
- Read directly
- Release tail

## 3. Python Integration
NumPy views over shared memory via PyBind11 and buffer protocol.

## 4. Memory Layout & Alignment
Atomic variables aligned to 64-byte boundaries to avoid false sharing.
