"""
Extract the raw RISC-V Linux Image from an unpacked kernel package.

Usage: extract_kernel.py <vmlinuz> <output Image>

The package ships the kernel gzip-compressed, while QEMU needs the raw Image.
"""

import gzip
import sys

IMAGE_MAGIC_OFFSET = 0x38
IMAGE_MAGIC = b"RSC\x05"
GZIP_MAGIC = b"\x1f\x8b"


def main():
    with open(sys.argv[1], "rb") as vmlinuz:
        data = vmlinuz.read()

    if data[:2] == GZIP_MAGIC:
        data = gzip.decompress(data)

    if data[IMAGE_MAGIC_OFFSET : IMAGE_MAGIC_OFFSET + len(IMAGE_MAGIC)] != IMAGE_MAGIC:
        sys.exit("unexpected kernel format")

    with open(sys.argv[2], "wb") as image:
        image.write(data)


if __name__ == "__main__":
    main()
