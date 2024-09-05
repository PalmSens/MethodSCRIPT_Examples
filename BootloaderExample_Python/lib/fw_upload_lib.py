#!/usr/bin/env python3
"""
Library for programming a binary file using the PalmSens bootloader for MethodSCRIPT devices.
"""

from pathlib import Path
from io import RawIOBase


def program(binary: Path, channel: RawIOBase):
    """Download a firmware binary to a PalmSens bootloader

    binary: The binary to upload

    channel: The binary communications channel to use
    """


    def tx_and_rx(channel: RawIOBase, data: bytes):
        """Transmit data and check reply does not error

        `extra_lines` can be used if the device returns multiple lines in
        response to a single command (like `t`)
        """
        channel.write(data)

        ret_data = channel.readline()
        if b"!" in ret_data:
            msg = f"Got device error {ret_data}, after sending {data.decode('utf8')}"
            raise RuntimeError(msg)


    def fletcher(inp: bytes) -> int:
        """Compute the fletcher CRC for a byte stream"""
        sum1 = 0
        sum2 = 0
        for byte in inp:
            sum1 = (sum1 + byte) % 255
            sum2 = (sum1 + sum2) % 255
        return (sum2 << 8) | sum1


    # End a previous download if there is one. Just in case.
    try:
        tx_and_rx(channel, b"endfw\n")
    except RuntimeError:
        pass

    print("Erasing")
    tx_and_rx(channel, b"startfw\n")

    with open(binary, "rb") as fw_file:
        index = 0
        while(chunk := fw_file.read(50)):
            index = index + 1
            payload = (
                f"{len(chunk):02x}".encode("ascii")
                + chunk.hex().encode("ascii")
                + f"{fletcher(chunk):04x}".encode("ascii")
            )
            tx_and_rx(channel, b"data" + payload + b"\n")
            print(f"Uploading {index}\r", end='', flush=True)

    print("\nFinishing upload")
    tx_and_rx(channel, b"endfw\n")

    print("Rebooting")
    tx_and_rx(channel, b"boot\n")
