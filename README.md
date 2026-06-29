# BeaconNode


BeaconNode is a high-performance, decentralized **Agent Resource Discovery (ARD)** engine designed for multi-agent systems. It pairs a C++20 background engine with a lightweight Python SDK, allowing AI agents to announce capabilities, discover peers, and converge on a shared registry without managing complex P2P protocol logic.

## Core Features
* **Decentralized Discovery**: Gossip-based synchronization (Push-Pull model) ensures eventual consistency across the network.
* **Identity Hardening**: Every registry entry is cryptographically signed using **Ed25519** (`libsodium`), ensuring tamper-proof agent registrations.
* **Persistence**: Durable registry state via **Write-Ahead Logging (WAL)** and binary snapshots, ensuring state recovery after crashes.
* **High Performance**: Built in C++20 with `boost::asio` for asynchronous I/O and `Protocol Buffers` for efficient communication.
* **Python SDK**: Idiomatic interface for rapid agent development with automatic signature handling.

## Components
- `beacon-engine`: C++20 service handling peer gossip, signature verification, and persistent storage.
- `beacon-sdk`: Python package for secure registration and agent discovery.
- `proto`: Shared definitions for registry entries and gossip messages.

## Build & Run
BeaconNode uses **CMake** and is optimized for the **MSYS2/UCRT64** toolchain.

### Build
```powershell
cmake -S . -B build-msys -G Ninja \
    -DBEACONNODE_ENABLE_ASIO_NETWORKING=ON \
    -DBEACONNODE_ENABLE_LIBSODIUM=ON \
    -DBEACONNODE_ENABLE_PROTOBUF=ON

cmake --build build-msys
