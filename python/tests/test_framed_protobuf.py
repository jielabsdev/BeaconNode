import socket
import threading

from beaconnode import AgentRegistration, BeaconClient
from beaconnode.framing import FRAME_CONTROL_RESPONSE, Frame, decode_frame, encode_frame
from beaconnode.proto.beaconnode.v1 import registry_pb2


def test_register_uses_framed_protobuf_control_request() -> None:
    ready = threading.Event()
    captured: list[registry_pb2.ControlRequest] = []
    port_holder: list[int] = []

    def server() -> None:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen(1)
            port_holder.append(listener.getsockname()[1])
            ready.set()
            conn, _ = listener.accept()
            with conn:
                data = conn.recv(4096)
                frame = decode_frame(data)
                request = registry_pb2.ControlRequest()
                request.ParseFromString(frame.payload)
                captured.append(request)

                response = registry_pb2.ControlResponse(ok=True, message="registered")
                conn.sendall(encode_frame(Frame(FRAME_CONTROL_RESPONSE, response.SerializeToString())))

    threading.Thread(target=server, daemon=True).start()
    ready.wait(timeout=2)

    client = BeaconClient(port=port_holder[0])
    result = client.register(
        AgentRegistration(
            agent_id="agent-1",
            node_id="node-1",
            endpoint="127.0.0.1:9000",
            metadata={"role": "planner"},
        )
    )

    assert result["ok"] is True
    assert captured[0].register_agent.agent_id == "agent-1"
    assert captured[0].register_agent.metadata["role"] == "planner"

