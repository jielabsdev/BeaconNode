import socket
import threading

from beaconnode import BeaconClient, sync_with_peer


class ScriptedServer:
    def __init__(self, responses: list[str]) -> None:
        self.responses = responses
        self.commands: list[str] = []
        self.port = 0
        self._ready = threading.Event()

    def start(self) -> None:
        thread = threading.Thread(target=self._serve, daemon=True)
        thread.start()
        self._ready.wait(timeout=2)

    def _serve(self) -> None:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen(len(self.responses))
            self.port = listener.getsockname()[1]
            self._ready.set()

            for response in self.responses:
                conn, _ = listener.accept()
                with conn:
                    self.commands.append(conn.recv(4096).decode("utf-8").strip())
                    conn.sendall(response.encode("utf-8"))


def test_sync_with_peer_pulls_and_pushes_revision_deltas() -> None:
    local = ScriptedServer(
        [
            "OK active_agents=1 highest_revision=2\n",
            "OK message=merged\n",
            "OK message=sync_pull\n"
            "AGENT agent_id=local-new node_id=node-a endpoint=127.0.0.1:9000 revision=5\n"
            "END\n",
        ]
    )
    remote = ScriptedServer(
        [
            "OK active_agents=1 highest_revision=3\n",
            "OK message=sync_pull\n"
            "AGENT agent_id=remote-new node_id=node-b endpoint=127.0.0.1:9001 revision=4\n"
            "END\n",
            "OK message=merged\n",
        ]
    )
    local.start()
    remote.start()

    result = sync_with_peer(BeaconClient(port=local.port, protocol="line"), BeaconClient(port=remote.port, protocol="line"))

    assert result.pulled == 1
    assert result.pushed == 1
    assert any(command.startswith("MERGE agent_id=remote-new") for command in local.commands)
    assert any(command.startswith("MERGE agent_id=local-new") for command in remote.commands)
