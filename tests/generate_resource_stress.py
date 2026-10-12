#!/usr/bin/env python3
"""Generate the 48 MP compressed neighbor outside the resource probe process.

Streaming PNG encoding uses a single 24 KB row rather than a decoded bitmap.
The output is a valid opaque black RGB PNG with an 8000 x 6000 pixel header.
"""
import argparse
import binascii
from pathlib import Path
import struct
import zlib


def chunk(file, kind, payload):
    file.write(struct.pack(">I", len(payload)))
    file.write(kind)
    file.write(payload)
    file.write(struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    output = parser.parse_args().output
    output.parent.mkdir(parents=True, exist_ok=True)
    row = b"\0" * (8000 * 3 + 1)
    compressor = zlib.compressobj(level=9)
    with output.open("wb") as file:
        file.write(b"\x89PNG\r\n\x1a\n")
        chunk(file, b"IHDR", struct.pack(">IIBBBBB", 8000, 6000, 8, 2, 0, 0, 0))
        for _ in range(6000):
            data = compressor.compress(row)
            if data:
                chunk(file, b"IDAT", data)
        chunk(file, b"IDAT", compressor.flush())
        chunk(file, b"IEND", b"")
    print(f"{output}: 8000x6000, {output.stat().st_size} compressed bytes, 192000000 decoded RGBA bytes")


if __name__ == "__main__":
    main()
