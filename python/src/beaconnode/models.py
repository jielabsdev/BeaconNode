from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(slots=True)
class AgentRegistration:
    agent_id: str
    node_id: str
    endpoint: str
    metadata: dict[str, str] = field(default_factory=dict)
    revision: int = 1
    public_key: bytes = b""
    signature: bytes = b""

    @property
    def public_key_hex(self) -> str:
        return self.public_key.hex()

    @property
    def signature_hex(self) -> str:
        return self.signature.hex()
