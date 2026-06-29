from __future__ import annotations

import struct
from dataclasses import dataclass


MAGIC = b"BN"
VERSION = 1
HEADER_SIZE = 9

FRAME_CONTROL_REQUEST = 1
FRAME_CONTROL_RESPONSE = 2
FRAME_GOSSIP_ENVELOPE = 3


@dataclass(slots=True)
class Frame:
    kind: int
    payload: bytes


def encode_frame(frame: Frame) -> bytes:
    return MAGIC + bytes([VERSION]) + struct.pack(">HI", frame.kind, len(frame.payload)) + frame.payload


def decode_frame(data: bytes) -> Frame:
    if len(data) < HEADER_SIZE:
        raise ValueError("frame is too short")
    if data[:2] != MAGIC:
        raise ValueError("invalid frame magic")
    if data[2] != VERSION:
        raise ValueError("unsupported frame version")

    kind, length = struct.unpack(">HI", data[3:HEADER_SIZE])
    if len(data) != HEADER_SIZE + length:
        raise ValueError("frame length mismatch")
    return Frame(kind=kind, payload=data[HEADER_SIZE:])


def expected_frame_length(header: bytes) -> int:
    if len(header) != HEADER_SIZE:
        raise ValueError("header must be exactly 9 bytes")
    if header[:2] != MAGIC:
        raise ValueError("invalid frame magic")
    _, length = struct.unpack(">HI", header[3:HEADER_SIZE])
    return HEADER_SIZE + length

