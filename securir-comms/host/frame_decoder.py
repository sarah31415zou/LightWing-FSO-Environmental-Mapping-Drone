"""Validated frame decoding for the SecurIR optical-link protocol.

The receiver firmware emits line-oriented telemetry, while integration tests and
companion computers often receive a stream of raw protocol bytes.  This module
turns that stream into checked :class:`protocol.Frame` objects and keeps useful
resynchronization counters for link diagnostics.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Final

from protocol import (
    MAX_PAYLOAD,
    PREAMBLE_BYTE,
    PREAMBLE_LEN,
    SYNC_HI,
    SYNC_LO,
    Frame,
    crc16_ccitt,
)

_HEADER_LEN: Final[int] = 3
_CRC_LEN: Final[int] = 2
_SYNC: Final[bytes] = bytes([PREAMBLE_BYTE]) * PREAMBLE_LEN + bytes([SYNC_HI, SYNC_LO])
_MIN_FRAME_LEN: Final[int] = len(_SYNC) + _HEADER_LEN + _CRC_LEN


class FrameDecodeError(ValueError):
    """Raised when a raw frame has an invalid layout or checksum."""


@dataclass(frozen=True)
class DecoderStats:
    """Counters that describe stream quality since a decoder was created."""

    frames: int
    bytes_discarded: int
    crc_failures: int
    length_failures: int


def decode_raw_frame(raw: bytes) -> Frame:
    """Decode one complete raw frame and verify its CRC16-CCITT checksum."""

    if len(raw) < _MIN_FRAME_LEN:
        raise FrameDecodeError("frame is shorter than the protocol minimum")
    if not raw.startswith(_SYNC):
        raise FrameDecodeError("frame preamble or synchronization word is invalid")

    body_start = len(_SYNC)
    seq, flags, payload_len = raw[body_start : body_start + _HEADER_LEN]
    if payload_len > MAX_PAYLOAD:
        raise FrameDecodeError("payload length exceeds protocol maximum")

    expected_len = body_start + _HEADER_LEN + payload_len + _CRC_LEN
    if len(raw) != expected_len:
        raise FrameDecodeError(
            f"frame length is {len(raw)}, expected {expected_len} for payload length {payload_len}"
        )

    body = raw[body_start : body_start + _HEADER_LEN + payload_len]
    received_crc = int.from_bytes(raw[-_CRC_LEN:], "big")
    calculated_crc = crc16_ccitt(body)
    if received_crc != calculated_crc:
        raise FrameDecodeError(
            f"CRC mismatch: received 0x{received_crc:04X}, calculated 0x{calculated_crc:04X}"
        )

    return Frame(seq=seq, flags=flags, payload=body[_HEADER_LEN:])


class StreamFrameDecoder:
    """Recover valid frames from arbitrary chunks of a raw byte stream.

    Noise before a preamble, truncated chunks, malformed lengths, and checksum
    failures are handled without losing a possible later frame boundary.
    """

    def __init__(self) -> None:
        self._buffer = bytearray()
        self._frames = 0
        self._bytes_discarded = 0
        self._crc_failures = 0
        self._length_failures = 0

    @property
    def stats(self) -> DecoderStats:
        return DecoderStats(
            frames=self._frames,
            bytes_discarded=self._bytes_discarded,
            crc_failures=self._crc_failures,
            length_failures=self._length_failures,
        )

    def reset(self) -> None:
        """Clear buffered bytes and diagnostic counters."""

        self._buffer.clear()
        self._frames = 0
        self._bytes_discarded = 0
        self._crc_failures = 0
        self._length_failures = 0

    def feed(self, chunk: bytes) -> list[Frame]:
        """Append *chunk* and return every complete, CRC-valid frame recovered."""

        self._buffer.extend(chunk)
        recovered: list[Frame] = []

        while True:
            sync_at = self._buffer.find(_SYNC)
            if sync_at < 0:
                self._retain_possible_sync_prefix()
                break

            if sync_at:
                del self._buffer[:sync_at]
                self._bytes_discarded += sync_at

            if len(self._buffer) < _MIN_FRAME_LEN:
                break

            payload_len = self._buffer[len(_SYNC) + 2]
            if payload_len > MAX_PAYLOAD:
                del self._buffer[0]
                self._bytes_discarded += 1
                self._length_failures += 1
                continue

            frame_len = len(_SYNC) + _HEADER_LEN + payload_len + _CRC_LEN
            if len(self._buffer) < frame_len:
                break

            candidate = bytes(self._buffer[:frame_len])
            try:
                frame = decode_raw_frame(candidate)
            except FrameDecodeError as error:
                del self._buffer[0]
                self._bytes_discarded += 1
                if "CRC mismatch" in str(error):
                    self._crc_failures += 1
                else:
                    self._length_failures += 1
                continue

            del self._buffer[:frame_len]
            self._frames += 1
            recovered.append(frame)

        return recovered

    def _retain_possible_sync_prefix(self) -> None:
        """Keep only the longest buffered suffix that can start a sync word."""

        keep = 0
        upper = min(len(self._buffer), len(_SYNC) - 1)
        for size in range(upper, 0, -1):
            if self._buffer[-size:] == _SYNC[:size]:
                keep = size
                break

        discarded = len(self._buffer) - keep
        if discarded:
            del self._buffer[:discarded]
            self._bytes_discarded += discarded
