# Protocol Notes

BeaconNode uses protobuf contracts for all cross-language messages. The first contracts live in `proto/beaconnode/v1/registry.proto`.

Milestone 2 also includes a temporary line-based loopback protocol so the engine and Python SDK can communicate before generated protobuf bindings are wired in.

## Local Control Protocol

The local SDK connects to TCP `127.0.0.1:43171` and sends one newline-terminated command per connection.

```text
REGISTER agent_id=agent-1 node_id=node-1 endpoint=127.0.0.1:9000 revision=1 metadata.role=planner
HEARTBEAT agent_id=agent-1 revision=2
MERGE agent_id=agent-2 node_id=node-2 endpoint=127.0.0.1:9001 revision=3 metadata.role=worker
SYNC_PULL after_revision=2 limit=256
LIST
STATS
```

Responses begin with `OK` or `ERR`.

```text
OK message=registered
OK message=list
AGENT agent_id=agent-1 node_id=node-1 endpoint=127.0.0.1:9000 revision=2 metadata.role=planner
END
```

This protocol is intentionally simple and local-only. It should be replaced by protobuf-framed messages once the generated C++ and Python bindings are enabled.

## Framing

Milestone 3 adds a binary frame envelope for protobuf payloads:

```text
magic: 2 bytes, "BN"
version: 1 byte
kind: 2 bytes, big endian
payload_length: 4 bytes, big endian
payload: protobuf bytes
```

Frame kinds:

- `1`: control request
- `2`: control response
- `3`: gossip envelope

Generated protobuf bindings are now wired into the TCP control server and Python SDK. The temporary line protocol remains active as a compatibility/debug fallback.

## Push-Pull Sync

Local revision sync is now available through TCP control commands:

1. Ask peer for `STATS`.
2. Pull records newer than the local `highest_revision` with `SYNC_PULL`.
3. Merge pulled records locally with normal registry semantics.
4. Push local records newer than the peer `highest_revision` with `MERGE`.

Conflict handling is intentionally deterministic: newer or equal revisions replace older local records; stale revisions are rejected.

## UDP Peer Handshake

Nodes answer UDP hello packets on the P2P listen port.

```text
BEACON_HELLO node_id=node-2 listen_port=43170
BEACON_WELCOME node_id=dev-node registry_revision=12
```

## Agent Lifecycle

1. An agent submits a signed registration.
2. The local engine validates the signature and trust policy.
3. The registration is stored with an expiry derived from the heartbeat TTL.
4. Heartbeats refresh the expiry.
5. Expired agents are purged by the lifecycle manager.

## Gossip

The gossip layer uses an epidemic push-pull model:

- push: send a bounded subset of local registry entries to selected peers
- pull: request entries newer than the receiver's known revision
- converge: merge valid entries by revision and expiry

## Trust

The trust engine starts with threshold counters for malformed payloads and excessive request rates. Future versions can add reputation persistence and peer scoring.
