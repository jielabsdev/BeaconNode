from __future__ import annotations

import base64
from dataclasses import replace
from pathlib import Path

from .models import AgentRegistration


class IdentityError(RuntimeError):
    pass


def canonical_agent_payload(registration: AgentRegistration) -> bytes:
    lines = [
        f"agent_id={registration.agent_id}",
        f"node_id={registration.node_id}",
        f"endpoint={registration.endpoint}",
        f"revision={registration.revision}",
    ]
    for key, value in sorted(registration.metadata.items()):
        lines.append(f"metadata.{key}={value}")
    return ("\n".join(lines) + "\n").encode("utf-8")


class AgentIdentity:
    def __init__(self, private_key: object, public_key: bytes | None = None) -> None:
        self._private_key = private_key
        self._public_key = public_key

    @classmethod
    def generate(cls) -> "AgentIdentity":
        ed25519, _, _ = _crypto_modules()
        private_key = ed25519.Ed25519PrivateKey.generate()
        public_key = private_key.public_key().public_bytes(
            raw_encoding(), raw_format()
        )
        return cls(private_key, public_key)

    @classmethod
    def load_or_create(cls, path: str | Path) -> "AgentIdentity":
        path = Path(path)
        if path.exists():
            return cls.load(path)

        identity = cls.generate()
        identity.save(path)
        return identity

    @classmethod
    def load(cls, path: str | Path) -> "AgentIdentity":
        _, serialization, _ = _crypto_modules()
        data = Path(path).read_bytes()
        private_key = serialization.load_pem_private_key(data, password=None)
        public_key = private_key.public_key().public_bytes(
            serialization.Encoding.Raw,
            serialization.PublicFormat.Raw,
        )
        return cls(private_key, public_key)

    def save(self, path: str | Path) -> None:
        _, serialization, _ = _crypto_modules()
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        data = self._private_key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.PKCS8,
            encryption_algorithm=serialization.NoEncryption(),
        )
        path.write_bytes(data)

    @property
    def public_key(self) -> bytes:
        if self._public_key is not None:
            return self._public_key
        _, serialization, _ = _crypto_modules()
        self._public_key = self._private_key.public_key().public_bytes(
            serialization.Encoding.Raw,
            serialization.PublicFormat.Raw,
        )
        return self._public_key

    def sign(self, registration: AgentRegistration) -> AgentRegistration:
        signature = self._private_key.sign(canonical_agent_payload(registration))
        return replace(registration, public_key=self.public_key, signature=signature)


def verify_registration(registration: AgentRegistration) -> bool:
    ed25519, _, exc_cls = _crypto_modules()
    if not registration.public_key or not registration.signature:
        return False
    public_key = ed25519.Ed25519PublicKey.from_public_bytes(registration.public_key)
    try:
        public_key.verify(registration.signature, canonical_agent_payload(registration))
        return True
    except exc_cls:
        return False


def encode_identity_public_key(identity: AgentIdentity) -> str:
    return base64.b64encode(identity.public_key).decode("ascii")


def raw_encoding():
    _, serialization, _ = _crypto_modules()
    return serialization.Encoding.Raw


def raw_format():
    _, serialization, _ = _crypto_modules()
    return serialization.PublicFormat.Raw


def _crypto_modules() -> tuple[object, object, object]:
    try:
        from cryptography.exceptions import InvalidSignature
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric import ed25519
    except ImportError as error:
        raise IdentityError(
            "Ed25519 support requires the optional runtime dependency: cryptography"
        ) from error

    return ed25519, serialization, InvalidSignature
