#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = []
# ///
"""
Generate damaged H.264 MPEG-TS test videos in test/ for the CLI tests.

Needs ffmpeg with libx264 in the PATH. Each clip is based on a 2-second,
25 fps, 320x240 test pattern:

- test-libx264-pts-jump.ts: two copies joined, timestamps jump forward by 100 s
- test-libx264-pts-backwards.ts: two copies joined, timestamps jump back by 5 s
- test-libx264-pts-wrap.ts: 33-bit timestamps wrap around after 1 s
- test-libx264-pmt-mpeg2.ts: the PMT declares the video stream as MPEG-2
- test-libx264-bitflip.ts: 100 random bit flips in the video payload
"""

import random
import subprocess
import tempfile
from pathlib import Path

TEST_DIR = Path(__file__).resolve().parent.parent / "test"
PACKET_SIZE = 188


def encode(offset: float) -> bytes:
    """Encode the test pattern to MPEG-TS, with timestamps shifted by offset seconds."""
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "out.ts"
        subprocess.run(
            [
                "ffmpeg", "-y", "-v", "error",
                "-f", "lavfi", "-i", "testsrc=duration=2:size=320x240:rate=25",
                "-c:v", "libx264", "-g", "25", "-bf", "2", "-pix_fmt", "yuv420p",
                "-output_ts_offset", str(offset),
                "-f", "mpegts", str(out),
            ],
            check=True,
        )  # fmt: skip
        return out.read_bytes()


def crc32_mpeg2(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte << 24
        for _ in range(8):
            crc = ((crc << 1) ^ 0x04C11DB7) if crc & 0x80000000 else crc << 1
            crc &= 0xFFFFFFFF
    return crc


def pid_of(packet: bytes) -> int:
    return ((packet[1] & 0x1F) << 8) | packet[2]


def payload_offset(packet: bytes) -> int:
    """Offset of the payload, after the header and the adaptation field."""
    if packet[3] & 0x20:
        return 5 + packet[4]
    return 4


def packets(data: bytes) -> list[bytearray]:
    return [bytearray(data[i : i + PACKET_SIZE]) for i in range(0, len(data), PACKET_SIZE)]


def section_of(packet: bytes) -> tuple[int, int]:
    """Start and length of the PSI section in a packet that starts one."""
    start = payload_offset(packet) + 1 + packet[payload_offset(packet)]
    length = 3 + (((packet[start + 1] & 0x0F) << 8) | packet[start + 2])
    return start, length


def pmt_pid(pkts: list[bytearray]) -> int:
    for packet in pkts:
        if pid_of(packet) == 0 and packet[1] & 0x40:
            start, _ = section_of(packet)
            # First program after the 8-byte section header
            return ((packet[start + 10] & 0x1F) << 8) | packet[start + 11]
    raise ValueError("no PAT found")


def set_video_stream_type(data: bytes, stream_type: int) -> bytes:
    """Set the stream type of the H.264 stream in every PMT and fix the CRC."""
    pkts = packets(data)
    pid = pmt_pid(pkts)
    for packet in pkts:
        if pid_of(packet) != pid or not packet[1] & 0x40:
            continue
        start, length = section_of(packet)
        program_info_length = ((packet[start + 10] & 0x0F) << 8) | packet[start + 11]
        pos = start + 12 + program_info_length
        end = start + length - 4
        while pos < end:
            if packet[pos] == 0x1B:
                packet[pos] = stream_type
            pos += 5 + (((packet[pos + 3] & 0x0F) << 8) | packet[pos + 4])
        crc = crc32_mpeg2(bytes(packet[start:end]))
        packet[end : end + 4] = crc.to_bytes(4, "big")
    return b"".join(pkts)


def flip_bits(data: bytes, count: int, seed: int) -> bytes:
    """Flip random bits in the payload of video packets that do not start a PES packet."""
    pkts = packets(data)
    candidates = [p for p in pkts if pid_of(p) == 0x100 and not p[1] & 0x40]
    rng = random.Random(seed)
    for _ in range(count):
        packet = rng.choice(candidates)
        pos = rng.randrange(payload_offset(packet), PACKET_SIZE)
        packet[pos] ^= 1 << rng.randrange(8)
    return b"".join(pkts)


def main() -> None:
    clean = encode(10)
    # 2**33 / 90000 is the timestamp range in seconds; ffmpeg starts at 1.4 s
    wrap_offset = 2**33 / 90000 - 2.4
    clips = {
        "test-libx264-pts-jump.ts": clean + encode(110),
        "test-libx264-pts-backwards.ts": clean + encode(7),
        "test-libx264-pts-wrap.ts": encode(wrap_offset),
        "test-libx264-pmt-mpeg2.ts": set_video_stream_type(clean, 0x02),
        "test-libx264-bitflip.ts": flip_bits(clean, 100, seed=1),
    }
    for name, data in clips.items():
        (TEST_DIR / name).write_bytes(data)
        print(f"Wrote test/{name} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
