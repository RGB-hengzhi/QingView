#!/usr/bin/env python3
"""Declare ditto's UTF-8 filenames for ZIP readers outside Archive Utility."""
import struct
import sys
from pathlib import Path
import zipfile


def fix(path):
    data = bytearray(path.read_bytes())
    with zipfile.ZipFile(path) as archive:
        position = archive.start_dir
        for item in archive.infolist():
            if data[position:position + 4] != b"PK\x01\x02":
                raise ValueError("Unexpected ZIP directory header")
            name_size, extra_size, comment_size = struct.unpack_from("<HHH", data, position + 28)
            name = bytes(data[position + 46:position + 46 + name_size])
            name.decode("utf-8")
            if any(byte >= 128 for byte in name):
                if data[item.header_offset:item.header_offset + 4] != b"PK\x03\x04":
                    raise ValueError("Unexpected ZIP local header")
                for offset in (position + 8, item.header_offset + 6):
                    flags = struct.unpack_from("<H", data, offset)[0]
                    struct.pack_into("<H", data, offset, flags | 0x800)
            position += 46 + name_size + extra_size + comment_size
    path.write_bytes(data)
    with zipfile.ZipFile(path) as archive:
        if archive.testzip() is not None:
            raise ValueError("ZIP content verification failed")


if __name__ == "__main__":
    fix(Path(sys.argv[1]))
