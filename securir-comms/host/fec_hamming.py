from __future__ import annotations


def encode_nibble(nibble: int) -> int:
    d1 = (nibble >> 0) & 1
    d2 = (nibble >> 1) & 1
    d3 = (nibble >> 2) & 1
    d4 = (nibble >> 3) & 1
    p1 = d1 ^ d2 ^ d4
    p2 = d1 ^ d3 ^ d4
    p3 = d2 ^ d3 ^ d4
    return (p1 << 0) | (p2 << 1) | (d1 << 2) | (p3 << 3) | (d2 << 4) | (d3 << 5) | (d4 << 6)


def decode_codeword(codeword: int) -> tuple[int, bool]:
    b1 = (codeword >> 0) & 1
    b2 = (codeword >> 1) & 1
    b3 = (codeword >> 2) & 1
    b4 = (codeword >> 3) & 1
    b5 = (codeword >> 4) & 1
    b6 = (codeword >> 5) & 1
    b7 = (codeword >> 6) & 1

    s1 = b1 ^ b3 ^ b5 ^ b7
    s2 = b2 ^ b3 ^ b6 ^ b7
    s3 = b4 ^ b5 ^ b6 ^ b7
    syndrome = s1 | (s2 << 1) | (s3 << 2)

    corrected = False
    if 1 <= syndrome <= 7:
        codeword ^= (1 << (syndrome - 1))
        corrected = True

    d1 = (codeword >> 2) & 1
    d2 = (codeword >> 4) & 1
    d3 = (codeword >> 5) & 1
    d4 = (codeword >> 6) & 1
    nibble = d1 | (d2 << 1) | (d3 << 2) | (d4 << 3)
    return nibble, corrected


def encode_bytes(data: bytes) -> bytes:
    out = bytearray()
    for b in data:
        lo = b & 0x0F
        hi = (b >> 4) & 0x0F
        out.append(encode_nibble(lo))
        out.append(encode_nibble(hi))
    return bytes(out)


def decode_bytes(data: bytes) -> tuple[bytes, int]:
    out = bytearray()
    corrected_count = 0
    if len(data) % 2:
        data = data[:-1]
    for i in range(0, len(data), 2):
        lo, c1 = decode_codeword(data[i] & 0x7F)
        hi, c2 = decode_codeword(data[i + 1] & 0x7F)
        corrected_count += int(c1) + int(c2)
        out.append((hi << 4) | lo)
    return bytes(out), corrected_count
