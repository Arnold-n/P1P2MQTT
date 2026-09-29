#!/usr/bin/env python3

"""Validate the confirmed ERSQ016AV1 + EKHBRD016ADV1 heat/DHW matrix.

Uses the exact captured frames where they are available and short normalized
fixtures for the remaining matrix states where only the confirmed family prefix
is currently documented.
"""

from __future__ import annotations


def payload_from_frame(frame: str) -> bytes:
    data = bytes.fromhex(frame)
    if len(data) < 5:
        raise ValueError(f"frame too short: {frame}")
    return data[3:-1]


def decode_request(frame: str) -> tuple[bool, bool]:
    payload = payload_from_frame(frame)
    return bool(payload[0] & 0x01), bool(payload[2] & 0x01)


def decode_response(frame: str) -> tuple[bool, bool]:
    payload = payload_from_frame(frame)
    return bool(payload[0] & 0x01), bool(payload[3] & 0x01)


REQUEST_CASES = {
    "00001001010130000A0020000000000200004001B1": (True, True),
    "00001001010030000A002000000000020000400131": (True, False),
    "00001000010100": (False, True),
    "00001000010000": (False, False),
}

RESPONSE_CASES = {
    "4000100100110130000A00200018000000000002000939": (True, True),
    "4000100100110030000A002000180000000000020009E0": (True, False),
    "4000100000110100": (False, True),
    "4000100000110000": (False, False),
}

ACK_FRAME = "80001001010030000A0020000000000200000001A4"


def main() -> int:
    for frame, expected in REQUEST_CASES.items():
        actual = decode_request(frame)
        assert actual == expected, f"request {frame}: expected {expected}, got {actual}"

    for frame, expected in RESPONSE_CASES.items():
        actual = decode_response(frame)
        assert actual == expected, f"response {frame}: expected {expected}, got {actual}"

    ack_payload = payload_from_frame(ACK_FRAME)
    assert ack_payload[0] & 0x01, "ack frame should carry heat=on in payload byte 0 bit 0"
    assert not (ack_payload[2] & 0x01), "ack frame should carry DHW=off in payload byte 2 bit 0"

    print("ERSQ016AV1/EKHBRD016ADV1 matrix validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
