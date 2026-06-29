import socket
import threading

from beaconnode import AgentRegistration, BeaconClient


def _serve_once(response: str) -> int:
    ready = threading.Event()
    port_holder: list[int] = []

    def server() -> None:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen(1)
            port_holder.append(listener.getsockname()[1])
            ready.set()
            conn, _ = listener.accept()
            with conn:
                conn.recv(4096)
                conn.sendall(response.encode("utf-8"))

    thread = threading.Thread(target=server, daemon=True)
    thread.start()
    ready.wait(timeout=2)
    return port_holder[0]


def test_register_sends_line_protocol_and_parses_status() -> None:
    port = _serve_once("OK message=registered\n")
    client = BeaconClient(port=port, protocol="line")
    response = client.register(
        AgentRegistration(
            agent_id="agent-1",
            node_id="node-1",
            endpoint="127.0.0.1:9000",
            metadata={"role": "planner"},
        )
    )

    assert response["ok"] is True
    assert response["message"] == "registered"


def test_list_agents_parses_agent_lines() -> None:
    port = _serve_once(
        "OK message=list\n"
        "AGENT agent_id=agent-1 node_id=node-1 endpoint=127.0.0.1:9000 revision=3 metadata.role=planner\n"
        "END\n"
    )
    client = BeaconClient(port=port, protocol="line")

    agents = list(client.list_agents())

    assert len(agents) == 1
    assert agents[0].agent_id == "agent-1"
    assert agents[0].metadata == {"role": "planner"}


def test_sync_pull_uses_revision_cursor() -> None:
    port = _serve_once(
        "OK message=sync_pull\n"
        "AGENT agent_id=agent-2 node_id=node-2 endpoint=127.0.0.1:9001 revision=4\n"
        "END\n"
    )
    client = BeaconClient(port=port, protocol="line")

    agents = client.sync_pull(after_revision=2, limit=10)

    assert len(agents) == 1
    assert agents[0].revision == 4


def test_signed_fields_round_trip_from_agent_line() -> None:
    port = _serve_once(
        "OK message=list\n"
        "AGENT agent_id=agent-1 node_id=node-1 endpoint=127.0.0.1:9000 revision=1 public_key=00ff signature=abcd\n"
        "END\n"
    )
    client = BeaconClient(port=port, protocol="line")

    agents = list(client.list_agents())

    assert agents[0].public_key == bytes.fromhex("00ff")
    assert agents[0].signature == bytes.fromhex("abcd")
