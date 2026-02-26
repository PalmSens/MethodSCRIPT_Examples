#!/usr/bin/env python3
"""
PalmSens MethodSCRIPT example: simple Cyclic Voltammetry (CV) measurement

This example showcases how to run a Cyclic Voltammetry (CV) measurement on
a MethodSCRIPT capable PalmSens instrument, such as the EmStat Pico.

The following features are demonstrated in this example:
  - Connecting to the PalmSens instrument using the serial port.
  - Running a Cyclic Voltammetry (CV) measurement.
  - Receiving and interpreting the measurement data from the device.
  - Plotting the measurement data.

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
import datetime
import logging
import os
import os.path
import sys

# Third-party imports
import matplotlib.pyplot as plt

# Local imports
import palmsens.instrument
import palmsens.mscript
import palmsens.serialport


###############################################################################
# Start of configuration
###############################################################################

# COM port of the device (None = auto detect).
DEVICE_PORT = None
# Baud rate of the MethodSCRIPT device.
# None = guess, based on COM port auto-detect.
# Common values are 230400 for EmStat Pico or 921600 for EmStat4 or Nexus.
BAUD_RATE = None

# Location of MethodSCRIPT file to use.
MSCRIPT_FILE_PATH = 'scripts/example_cv_nscans.mscr'

# Location of output files. Directory will be created if it does not exist.
OUTPUT_PATH = 'output'

###############################################################################
# End of configuration
###############################################################################


LOG = logging.getLogger(__name__)


def main():
    """Run the example."""
    # Configure the logging.
    logging.basicConfig(level=logging.DEBUG, format='[%(module)s] %(message)s',
                        stream=sys.stdout)
    # Uncomment the following line to reduce the log level for our library.
    # logging.getLogger('palmsens').setLevel(logging.INFO)
    # Disable excessive logging from matplotlib.
    logging.getLogger('matplotlib').setLevel(logging.INFO)
    logging.getLogger('PIL.PngImagePlugin').setLevel(logging.INFO)

    port = DEVICE_PORT
    baudrate = BAUD_RATE
    if port is None:
        (port, guessed_baudrate) = palmsens.serialport.auto_detect_port()
        if BAUD_RATE is None:
            baudrate = guessed_baudrate

    if baudrate is None:
        LOG.error('Baud rate must be provided when not auto-detecting serial port')
        sys.exit()

    # Create and open serial connection to the device.
    with palmsens.serialport.Serial(port, baudrate, 1) as comm:
        device = palmsens.instrument.Instrument(comm)
        device_type = device.get_device_type()
        LOG.info('Connected to %s.', device_type)

        # Read and send the MethodSCRIPT file.
        LOG.info('Sending MethodSCRIPT.')
        device.send_script(MSCRIPT_FILE_PATH)

        # Read the result lines.
        LOG.info('Waiting for results.')
        result_lines = device.readlines_until_end()

    # Store results in file.
    os.makedirs(OUTPUT_PATH, exist_ok=True)
    result_file_name = datetime.datetime.now().strftime('ms_plot_cv_%Y%m%d-%H%M%S.txt')
    result_file_path = os.path.join(OUTPUT_PATH, result_file_name)
    with open(result_file_path, 'wt', encoding='ascii') as file:
        file.writelines(result_lines)

    # Parse the result.
    curves = palmsens.mscript.parse_result_lines(result_lines)

    # Log the results.
    for curve in curves.loops:
        for package in curve.packages:
            LOG.info([str(value) for value in package])

    # Plot the results.
    plt.figure(1)
    for icurve, curve in enumerate(curves.loops):
        applied_potential = curve.get_column_values(0)
        measured_current = curve.get_column_values(1)
        plt.plot(applied_potential, measured_current, label=f"Curve {icurve}")
    plt.title('Voltammogram')
    plt.xlabel('Applied Potential (V)')
    plt.ylabel('Measured Current (A)')
    plt.grid(visible=True, which='major', linestyle='-')
    plt.grid(visible=True, which='minor', linestyle='--', alpha=0.2)
    plt.ticklabel_format(axis='both', style='sci', scilimits=(0, 0), useOffset=False)
    plt.minorticks_on()
    plt.legend()
    plt.tight_layout()
    plt.show()


if __name__ == '__main__':
    main()
