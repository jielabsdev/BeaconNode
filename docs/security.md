# Security

BeaconNode registrations are designed to be signed with Ed25519.

## Canonical Payload

Agents sign this exact payload form:

```text
agent_id=<id>
node_id=<node>
endpoint=<host:port>
revision=<number>
metadata.<key>=<value>
```

Metadata keys are sorted lexicographically before signing.

## Verification

- Python SDK: uses `cryptography` Ed25519 keys.
- C++ engine: uses libsodium when `BEACONNODE_ENABLE_LIBSODIUM=ON`.
- Without libsodium, C++ signature verification returns `Unsupported`.

Production bootstrap and relay nodes should set:

```toml
[trust]
require_signatures = true
```

This fails closed if signature verification is unavailable, which prevents accidental unsigned public deployment.

