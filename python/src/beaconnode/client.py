from __future__ import annotations

from dataclasses import asdict
import socket
from typing import Iterable

from .framing import (
    FRAME_CONTROL_REQUEST,
    FRAME_CONTROL_RESPONSE,
    HEADER_SIZE,
    Frame,
    decode_frame,
    encode_frame,
    expected_frame_length,
)
from .identity import AgentIdentity
from .models import AgentRegistration


class BeaconConnectionError(RuntimeError):
    pass


class BeaconProtocolError(RuntimeError):
    pass


class BeaconClient:
    """Client for the local BeaconNode engine control endpoint."""

    def __init__(
        self,
        host: str = "127.0.0.1",
        port: int = 43171,
        protocol: str = "protobuf",
        identity: AgentIdentity | None = None,
    ) -> None:
        self.host = host
        self.port = port
        self.protocol = protocol
        self.identity = identity

    def register(self, registration: AgentRegistration) -> dict[str, object]:
        if self.identity is not None:
            registration = self.identity.sign(registration)
        if self.protocol == "protobuf":
            registry_pb2 = _registry_pb2()
            request = registry_pb2.ControlRequest(register_agent=_to_proto_agent(registration))
            return _parse_proto_status(self._round_trip_proto(request), request=asdict(registration))
        response = self._round_trip(_encode_register(registration))
        return _parse_status(response, request=asdict(registration))

    def heartbeat(self, agent_id: str, revision: int = 1) -> dict[str, object]:
        if self.protocol == "protobuf":
            registry_pb2 = _registry_pb2()
            request = registry_pb2.ControlRequest(
                heartbeat=registry_pb2.Heartbeat(agent_id=agent_id, revision=revision)
            )
            return _parse_proto_status(self._round_trip_proto(request), agent_id=agent_id)
        response = self._round_trip(f"HEARTBEAT agent_id={agent_id} revision={revision}")
        return _parse_status(response, agent_id=agent_id)

    def list_agents(self) -> Iterable[AgentRegistration]:
        if self.protocol == "protobuf":
            registry_pb2 = _registry_pb2()
            request = registry_pb2.ControlRequest(list_agents=registry_pb2.ListAgentsRequest())
            return _from_proto_agents(self._round_trip_proto(request).snapshot.agents)
        response = self._round_trip("LIST")
        return _parse_agent_list(response)

    def sync_pull(self, after_revision: int = 0, limit: int = 256) -> list[AgentRegistration]:
        if self.protocol == "protobuf":
            registry_pb2 = _registry_pb2()
            request = registry_pb2.ControlRequest(
                sync_pull=registry_pb2.SyncPullRequest(after_revision=after_revision, limit=limit)
            )
            return _from_proto_agents(self._round_trip_proto(request).snapshot.agents)
        response = self._round_trip(f"SYNC_PULL after_revision={after_revision} limit={limit}")
        return _parse_agent_list(response)

    def merge(self, registration: AgentRegistration) -> dict[str, object]:
        if self.identity is not None:
            registration = self.identity.sign(registration)
        if self.protocol == "protobuf":
            registry_pb2 = _registry_pb2()
            request = registry_pb2.ControlRequest(sync_merge=_to_proto_agent(registration))
            return _parse_proto_status(self._round_trip_proto(request), request=asdict(registration))
        response = self._round_trip(_encode_merge(registration))
        return _parse_status(response, request=asdict(registration))

    def stats(self) -> dict[str, object]:
        if self.protocol == "protobuf":
            registry_pb2 = _registry_pb2()
            request = registry_pb2.ControlRequest(stats=registry_pb2.StatsRequest())
            response = self._round_trip_proto(request)
            return _parse_proto_status(response)
        response = self._round_trip("STATS")
        return _parse_status(response)

    def _round_trip(self, command: str) -> str:
        try:
            with socket.create_connection((self.host, self.port), timeout=3.0) as sock:
                sock.sendall((command + "\n").encode("utf-8"))
                return _read_all(sock)
        except OSError as error:
            raise BeaconConnectionError(
                f"could not connect to BeaconNode engine at {self.host}:{self.port}"
            ) from error

    def _round_trip_proto(self, request: registry_pb2.ControlRequest) -> registry_pb2.ControlResponse:
        registry_pb2 = _registry_pb2()
        payload = request.SerializeToString()
        framed = encode_frame(Frame(kind=FRAME_CONTROL_REQUEST, payload=payload))
        try:
            with socket.create_connection((self.host, self.port), timeout=3.0) as sock:
                sock.sendall(framed)
                header = _read_exact(sock, HEADER_SIZE)
                total_length = expected_frame_length(header)
                data = header + _read_exact(sock, total_length - HEADER_SIZE)
        except OSError as error:
            raise BeaconConnectionError(
                f"could not connect to BeaconNode engine at {self.host}:{self.port}"
            ) from error

        frame = decode_frame(data)
        if frame.kind != FRAME_CONTROL_RESPONSE:
            raise BeaconConnectionError("BeaconNode returned an unexpected protobuf frame")
        response = registry_pb2.ControlResponse()
        response.ParseFromString(frame.payload)
        return response


def _read_all(sock: socket.socket) -> str:
    chunks: list[bytes] = []
    while True:
        chunk = sock.recv(4096)
        if not chunk:
            break
        chunks.append(chunk)
    return b"".join(chunks).decode("utf-8")


def _read_exact(sock: socket.socket, length: int) -> bytes:
    chunks: list[bytes] = []
    remaining = length
    while remaining > 0:
        chunk = sock.recv(remaining)
        if not chunk:
            raise BeaconConnectionError("BeaconNode closed the connection mid-frame")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def _registry_pb2():
    try:
        from .proto.beaconnode.v1 import registry_pb2
    except Exception as error:
        raise BeaconProtocolError(
            "protobuf protocol requires generated BeaconNode protobuf bindings "
            "and a protobuf runtime compatible with protoc 7.35.0"
        ) from error
    return registry_pb2


def _to_proto_agent(registration: AgentRegistration) -> registry_pb2.AgentRegistration:
    registry_pb2 = _registry_pb2()
    proto = registry_pb2.AgentRegistration(
        agent_id=registration.agent_id,
        node_id=registration.node_id,
        endpoint=registration.endpoint,
        revision=registration.revision,
        public_key=registration.public_key,
        signature=registration.signature,
    )
    proto.metadata.update(registration.metadata)
    return proto


def _from_proto_agents(agents: Iterable[registry_pb2.AgentRegistration]) -> list[AgentRegistration]:
    return [
        AgentRegistration(
            agent_id=agent.agent_id,
            node_id=agent.node_id,
            endpoint=agent.endpoint,
            metadata=dict(agent.metadata),
            revision=agent.revision,
            public_key=agent.public_key,
            signature=agent.signature,
        )
        for agent in agents
    ]


def _parse_proto_status(response: registry_pb2.ControlResponse, **extra: object) -> dict[str, object]:
    result: dict[str, object] = {"ok": response.ok, "message": response.message, "endpoint": ""}
    result.update(_parse_fields("OK " + response.message if response.ok else "ERR " + response.message))
    result.update(extra)
    return result


def _encode_register(registration: AgentRegistration) -> str:
    return _encode_record_command("REGISTER", registration)


def _encode_merge(registration: AgentRegistration) -> str:
    return _encode_record_command("MERGE", registration)


def _encode_record_command(command: str, registration: AgentRegistration) -> str:
    parts = [
        command,
        f"agent_id={registration.agent_id}",
        f"node_id={registration.node_id}",
        f"endpoint={registration.endpoint}",
        f"revision={registration.revision}",
    ]

    for key, value in registration.metadata.items():
        parts.append(f"metadata.{key}={value}")
    if registration.public_key:
        parts.append(f"public_key={registration.public_key.hex()}")
    if registration.signature:
        parts.append(f"signature={registration.signature.hex()}")

    return " ".join(parts)


def _parse_status(response: str, **extra: object) -> dict[str, object]:
    first_line = response.splitlines()[0] if response else "ERR message=empty_response"
    fields = _parse_fields(first_line)
    result: dict[str, object] = {
        "ok": first_line.startswith("OK"),
        "endpoint": extra.pop("endpoint", None),
    }
    result.update(fields)
    result.update(extra)
    result["endpoint"] = result["endpoint"] or ""
    return result


def _parse_agent_list(response: str) -> list[AgentRegistration]:
    agents: list[AgentRegistration] = []
    for line in response.splitlines():
        if not line.startswith("AGENT "):
            continue

        fields = _parse_fields(line)
        metadata = {
            key.removeprefix("metadata."): value
            for key, value in fields.items()
            if key.startswith("metadata.")
        }
        agents.append(
            AgentRegistration(
                agent_id=str(fields.get("agent_id", "")),
                node_id=str(fields.get("node_id", "")),
                endpoint=str(fields.get("endpoint", "")),
                metadata=metadata,
                revision=int(fields.get("revision", 0)),
                public_key=bytes.fromhex(str(fields.get("public_key", ""))),
                signature=bytes.fromhex(str(fields.get("signature", ""))),
            )
        )

    return agents


def _parse_fields(line: str) -> dict[str, object]:
    fields: dict[str, object] = {}
    for token in line.split()[1:]:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        if key in {"public_key", "signature"}:
            fields[key] = value
        elif value.isdecimal():
            fields[key] = int(value)
        else:
            fields[key] = value

    return fields
