# BeaconNode Python SDK

The SDK exposes a small client API for applications that want to register agents, send heartbeats, and query the local BeaconNode engine.

The SDK connects to the engine with framed protobuf control messages by default. The older line protocol can still be selected with `BeaconClient(protocol="line")`.

```python
from beaconnode import AgentRegistration, BeaconClient

client = BeaconClient()
client.register(
    AgentRegistration(
        agent_id="agent-1",
        node_id="node-1",
        endpoint="127.0.0.1:9000",
        metadata={"role": "planner"},
    )
)
print(client.stats())
```
