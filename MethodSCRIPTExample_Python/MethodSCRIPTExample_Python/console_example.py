#!/usr/bin/env python3
"""
PalmSens MethodSCRIPT console example

This example demonstrates how to communicate with a MethodSCRIPT capable
PalmSens instrument, such as the EmStat Pico.

The following features are demonstrated in this example:
  - Auto detecting the serial port.
  - Connecting to the device using the serial port.
  - Reading the firmware version and device type.
  - Reading a MethodSCRIPT from file and executing it on the device. The
    MethodSCRIPT used in this example performs a Cyclic Voltammetry (CV)
    measurement.
  - Receiving and interpreting the response from the device (i.e., the data
    packages) and printing the measurement data to the console.

-------------------------------------------------------------------------------
Copyright (c) 2019-2021 PalmSens BV
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Neither the name of PalmSens BV nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.
   - This license does not release you from any requirement to obtain separate 
	  licenses from 3rd party patent holders to use this software.
   - Use of the software either in source or binary form must be connected to, 
	  run on or loaded to an PalmSens BV component.

DISCLAIMER: THIS SOFTWARE IS PROVIDED BY PALMSENS "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

# Standard library imports
import logging
import sys

# Local imports
import palmsens.instrument
import palmsens.mscript
import palmsens.serial


###############################################################################
# Start of configuration
###############################################################################

# COM port of the MethodSCRIPT device (None = auto detect).
# In case auto detection does not work or is not wanted, fill in the correct
# port name, e.g. 'COM6' on Windows, or '/dev/ttyUSB0' on Linux.
# DEVICE_PORT = 'COM6'
DEVICE_PORT = None

# Location of MethodSCRIPT file to use.
MSCRIPT_FILE_PATH = 'scripts/example_cv.mscr'

###############################################################################
# End of configuration
###############################################################################


LOG = logging.getLogger(__name__)


def main():
    """Run the example."""
    # Configure the logging.
    logging.basicConfig(level=logging.INFO, format='[%(module)s] %(message)s',
                        stream=sys.stdout)
    # Uncomment the following line to reduce the log level for our library.
    # logging.getLogger('palmsens').setLevel(logging.INFO)

    port = DEVICE_PORT
    if port is None:
        port = palmsens.serial.auto_detect_port()

    # Create and open serial connection to the device.
    LOG.info('Trying to connect to device using port %s...', port)
    with palmsens.serial.Serial(port, 5) as comm:
        device = palmsens.instrument.Instrument(comm)
        LOG.info('Connected.')

        # For development: abort any previous script and restore communication.
        device.abort_and_sync()

        # Check if device is connected and responding successfully.
        firmware_version = device.get_firmware_version()
        device_type = device.get_device_type()
        LOG.info('Connected to %s.', device_type)
        LOG.info('Firmware version: %s' % firmware_version)
        LOG.info('MethodSCRIPT version: %d' % device.get_mscript_version())
        LOG.info('Serial number = %s' % device.get_serial_number())

        # Read MethodSCRIPT from file and send to device.
        device.send_script(MSCRIPT_FILE_PATH)

        # Read the script output (results) from the device.
        while True:
            line = device.readline()

            # No data means timeout, so ignore it and try again.
            if not line:
                continue

            # An empty line means end of script.
            if line == '\n':
                break

            # Non-empty line received. Try to parse as data package.
            variables = palmsens.mscript.parse_mscript_data_package(line)

            if variables:
                # Apparently it was a data package. Print all variables.
                cols = []
                for var in variables:
                    cols.append('%s = %11.4g %s' % (var.type.name, var.value,
                                                    var.type.unit))
                    if 'status' in var.metadata:
                        status_text = palmsens.mscript.metadata_status_to_text(
                            var.metadata['status'])
                        cols.append('STATUS: %-16s' % status_text)
                    if 'cr' in var.metadata:
                        cr_text = palmsens.mscript.metadata_current_range_to_text(
                            device_type, var.type, var.metadata['cr'])
                        cols.append('CR: %s' % cr_text)
                print(' | '.join(cols))


if __name__ == '__main__':
    main()
