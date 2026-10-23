# bluefield-doca-cpp

A C++20 wrapper library for NVIDIA DOCA on BlueField DPUs.

`docacxx` provides C++ interfaces for DOCA Comch, DMA, LZ4/DEFLATE decompression, and erasure coding. It includes helpers for context management, asynchronous tasks, and registered memory to reduce boilerplate in host-to-DPU communication and hardware offload applications.

## How it works

```text
Application
    |
docacxx
Contexts / Tasks / Memory
    |
NVIDIA DOCA
Comch / DMA / Decompression / Erasure Coding
    |
BlueField DPU
```

- Comch wrappers support client/server connections and producer/consumer data transfers.
- `Context` manages the DOCA context lifecycle. Task wrappers provide completion callbacks, driven by the DOCA progress engine.
- `Mmap`, `Buffer`, and buffer inventories wrap memory registration and DOCA buffers, including PCI memory map export and import.
- `TaskContext` limits in-flight tasks and queues pending submissions until task slots become available.
