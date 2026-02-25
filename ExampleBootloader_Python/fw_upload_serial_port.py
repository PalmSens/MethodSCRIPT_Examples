#!/usr/bin/env python3
"""
Upload a firmware binary to a PalmSens device, using a serial port connection.
"""

# usage: fw_upload_serial_port.py [-h] [-f {off,soft,hard}] [-b BAUDRATE] binary port
# positional arguments:
#   binary                The binary to upload
#   port                  The COM port connected the bootloader
#
# options:
#   -h, --help            show this help message and exit
#   -f {off,soft,hard}, --flow_control {off,soft,hard}
#   -b BAUDRATE, --baudrate BAUDRATE

from pathlib import Path
import argparse
import serial
from lib.fw_upload_lib import program

SERIAL_TIMEOUT = 15

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "binary",
        type=Path,
        help="The binary to upload",
    )
    parser.add_argument(
        "port",
        type=str,
        help="The COM port connected the bootloader",
    )
    parser.add_argument(
        "-f",
        "--flow_control",
        choices=["off", "soft", "hard"],
        default="hard",
    )
    parser.add_argument(
        "-b",
        "--baudrate",
        default=230400,
    )

    args = parser.parse_args()
    program(
        args.binary,
        serial.Serial(
            args.port,
            args.baudrate,
            timeout=SERIAL_TIMEOUT,
            rtscts=(args.flow_control == "hard"),
            xonxoff=(args.flow_control == "soft"),
        ),
    )
