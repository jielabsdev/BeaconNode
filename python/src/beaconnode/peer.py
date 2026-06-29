from __future__ import annotations

import socket


def probe_peer(
    node_id: str,
    host: str = "127.0.0.1",
    port: int = 43170,
    listen_port: int = 43170,
    timeout: float = 3.0,
) -> dict[str, object]:
    payload = f"BEACON_HELLO node_id={node_id} listen_port={listen_port}".encode("utf-8")
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        sock.sendto(payload, (host, port))
        response, address = sock.recvfrom(1024)

    fields = _parse_fields(response.decode("utf-8"))
    return {
        "ok": response.startswith(b"BEACON_WELCOME"),
        "address": f"{address[0]}:{address[1]}",
        **fields,
    }


def _parse_fields(line: str) -> dict[str, object]:
    fields: dict[str, object] = {}
    for token in line.split()[1:]:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        fields[key] = int(value) if value.isdecimal() else value
    return fields
