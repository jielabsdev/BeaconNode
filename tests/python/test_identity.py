from beaconnode import AgentRegistration, canonical_agent_payload


def test_canonical_agent_payload_is_stable_and_sorted() -> None:
    registration = AgentRegistration(
        agent_id="agent-1",
        node_id="node-1",
        endpoint="127.0.0.1:9000",
        metadata={"z": "last", "a": "first"},
        revision=2,
    )

    assert canonical_agent_payload(registration) == (
        b"agent_id=agent-1\n"
        b"node_id=node-1\n"
        b"endpoint=127.0.0.1:9000\n"
        b"revision=2\n"
        b"metadata.a=first\n"
        b"metadata.z=last\n"
    )

