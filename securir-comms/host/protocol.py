from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

PREAMBLE_BYTE = 0x55
PREAMBLE_LEN = 8
SYNC_HI = 0xD3
SYNC_LO = 0x91
MAX_PAYLOAD = 200


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


@dataclass
class Frame:
    seq: int
    flags: int
    payload: bytes

    def encode_raw(self) -> bytes:
        if len(self.payload) > MAX_PAYLOAD:
            raise ValueError("payload too large")
        head = bytes([self.seq & 0xFF, self.flags & 0xFF, len(self.payload) & 0xFF])
        body = head + self.payload
        crc = crc16_ccitt(body)
        return (
            bytes([PREAMBLE_BYTE] * PREAMBLE_LEN)
            + bytes([SYNC_HI, SYNC_LO])
            + body
            + crc.to_bytes(2, "big")
        )


def manchester_encode(raw: bytes) -> bytes:
    # IEEE-like mapping: 0 -> 01, 1 -> 10
    out = bytearray()
    current = 0
    bit_pos = 0

    def push_bit(b: int) -> None:
        nonlocal current, bit_pos
        current = (current << 1) | (b & 1)
        bit_pos += 1
        if bit_pos == 8:
            out.append(current & 0xFF)
            current = 0
            bit_pos = 0

    for byte in raw:
        for i in range(7, -1, -1):
            bit = (byte >> i) & 1
            if bit == 0:
                push_bit(0)
                push_bit(1)
            else:
                push_bit(1)
                push_bit(0)

    if bit_pos:
        current <<= (8 - bit_pos)
        out.append(current & 0xFF)
    return bytes(out)


def manchester_decode(encoded: bytes, expected_bits: Optional[int] = None) -> bytes:
    bits = []
    for byte in encoded:
        for i in range(7, -1, -1):
            bits.append((byte >> i) & 1)

    if expected_bits is not None:
        bits = bits[:expected_bits]

    if len(bits) % 2:
        bits = bits[:-1]

    out_bits = []
    for i in range(0, len(bits), 2):
        pair = (bits[i], bits[i + 1])
        if pair == (0, 1):
            out_bits.append(0)
        elif pair == (1, 0):
            out_bits.append(1)
        else:
            out_bits.append(0)

    out = bytearray()
    cur = 0
    n = 0
    for bit in out_bits:
        cur = (cur << 1) | bit
        n += 1
        if n == 8:
            out.append(cur & 0xFF)
            cur = 0
            n = 0
    return bytes(out)


def parse_rx_line(line: str) -> dict:
    # Expected example:
    # RX seq=12 len=4 crc=1 ber_ppm=120 rssi=845 hex=41424344
    parts = line.strip().split()
    if not parts or parts[0] != "RX":
        return {}
    parsed: dict = {"type": "RX"}
    for p in parts[1:]:
        if "=" not in p:
            continue
        key, val = p.split("=", 1)
        parsed[key] = val
    return parsed
