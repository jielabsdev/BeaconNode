# Architecture

BeaconNode is a modular monolith: one deployable engine with clear internal boundaries.

## Beacon Engine

The C++20 engine owns long-running network and registry behavior:

- peer identity and endpoint tracking
- local agent registry
- heartbeat TTL cleanup
- push-pull gossip scheduling
- trust and spam filtering
- local control API for SDK clients

## Python SDK

The Python SDK is intentionally thin. It serializes requests, sends them to the local engine, and returns typed responses. The SDK does not implement P2P behavior.

## Bootstrap/Relay Layer

Bootstrap nodes are ordinary BeaconNode engines running with stable network identities and published endpoints. Relay behavior will be added as a transport capability once the first local P2P handshake is stable.

## Current Milestone

Milestone 2 establishes the first working local control path and UDP peer handshake:

- Python SDK sends one command per TCP loopback connection.
- C++ engine mutates and queries the registry through `ControlProtocol`.
- C++ engine answers UDP `BEACON_HELLO` messages with `BEACON_WELCOME`.

Milestone 3 adds protobuf-ready framing and revision-based push-pull synchronization. The line protocol still exists as the bootstrapping transport while generated protobuf classes are pending.

After milestone 3, two major milestones remain:

- Milestone 4: generated protobuf integration, persistent identities, and signed registration verification.
- Milestone 5: resilience testing, churn simulation, and bootstrap/relay deployment profile.
