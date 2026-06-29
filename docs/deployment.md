# Deployment

BeaconNode can run as ordinary agent nodes, bootstrap nodes, or relay nodes. Bootstrap and relay nodes should use stable DNS names, persistent identity storage, signature enforcement, and conservative rate limits.

## Bootstrap Profile

The bootstrap profile lives in `deploy/bootstrap/bootstrap-node.toml`.

Key settings:

- `listen_host = "0.0.0.0"` exposes the peer handshake port.
- `control_host = "127.0.0.1"` keeps the SDK control API private to the machine.
- `require_signatures = true` rejects unsigned registrations and gossip merges.
- `malformed_threshold = 3` quickly ignores peers sending bad payloads.

## Relay Profile

The relay profile lives in `deploy/bootstrap/relay-node.toml`. The first relay milestone uses the same engine behavior with a separate stable port and identity. NAT traversal relay forwarding should be implemented after the libp2p transport lands.

## Compose

```powershell
cd deploy\bootstrap
docker compose up -d
```

The image name is a placeholder until CI publishes a container. Locally, build and tag your engine image as `beaconnode/beacon-engine:latest`.

## Hardening Checklist

- Use persistent volumes for node identity.
- Restrict TCP control ports to loopback or a private management network.
- Publish only UDP/TCP peer ports.
- Enable `require_signatures`.
- Run at least three bootstrap nodes across distinct networks.
- Monitor malformed payload counts and registry churn.

