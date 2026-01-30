#!/usr/bin/env python3
"""
PalmSens MethodSCRIPT example: plot anything

This example showcases how to plot the result of most MethodScripts.

The following features are demonstrated in this example:
  - Connecting to the instrument using the serial port.
  - Running a MethodScript.
  - Receiving and interpreting the measurement data from the instrument.
  - Plotting the measurement data in plots.

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

import matplotlib.pyplot as plt

# Local imports
import palmsens.instrument
from palmsens import mscript
import palmsens.serialport


###############################################################################
# Start of configuration
###############################################################################

# COM port of the MethodSCRIPT device (None = auto-detect).
# In case auto-detection does not work or is not wanted, fill in the correct
# port name, e.g. 'COM6' on Windows, or '/dev/ttyUSB0' on Linux.
# DEVICE_PORT = 'COM6'
DEVICE_PORT = None
# Baud rate of the MethodSCRIPT device. If both this and DEVICE_PORT are None,
# the auto-detection mechanism will try to guess the correct baud rate. Setting
# BAUD_RATE overrides the guessed value if using COM-port autodetect.
# Common values are 230400 for EmStat Pico or 921600 for EmStat4 or Nexus.
BAUD_RATE = None

# Location of MethodSCRIPT file to use.
MSCRIPT_FILE_PATH = 'scripts/example_advanced_swv.mscr'

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
    baudrate = BAUD_RATE
    if port is None:
        (port, guessed_baudrate) = palmsens.serialport.auto_detect_port()
        if BAUD_RATE is None:
            baudrate = guessed_baudrate

    if baudrate is None:
        LOG.error('Baud rate must be provided when not auto-detecting serial port')
        sys.exit()

    # Create and open serial connection to the device.
    LOG.info('Trying to connect to device using port %s...', port)
    device_type = None
    with palmsens.serialport.Serial(port, baudrate, 5) as comm:
        device = palmsens.instrument.Instrument(comm)
        LOG.info('Connected.')

        # For development: abort any previous script and restore communication.
        device.abort_and_sync()

        # Check if device is connected and responding successfully.
        firmware_version = device.get_firmware_version()
        device_type = device.get_device_type()
        LOG.info('Connected to %s.', device_type)
        LOG.info('Firmware version: %s', firmware_version)
        LOG.info('MethodSCRIPT version: %s', device.get_mscript_version())
        LOG.info('Serial number = %s', device.get_serial_number())

        # Read MethodSCRIPT from file and send to device.
        device.send_script(MSCRIPT_FILE_PATH)

        LOG.info('Waiting for results.')
        result_lines = device.readlines_until_end()

    # Parse the result.
    data = mscript.parse_result_lines(result_lines)
    plot_loop(data, device_type)
    plt.show()


def plot_loop(loop: mscript.MScriptLoop, device_type):
    """ Plot all data points of a loop. """
    if loop.packages:
        # The first variables in the data packages are used as values for the x-axis.
        curve_x = loop.get_column(0)
        # Plot all consecutive variables in the data packages versus the first variable separately.
        for i in range(1, len(loop.packages[0])):
            curve_y = loop.get_column(i)
            plot_curve(curve_x, curve_y, device_type)

    # Also handle nested loops.
    for loop in loop.loops:
        plot_loop(loop, device_type)


def plot_curve(curve_x: tuple[mscript.MScriptVar], curve_y: tuple[mscript.MScriptVar], device_type):
    # Get the value from MethodSCRIPT variables and plot these in a new figure.
    x_vals = [p.value for p in curve_x]
    y_vals = [p.value for p in curve_y]
    plt.figure()
    plt.plot(x_vals, y_vals)
    plt.title(curve_y[0].type.name)
    plt.xlabel(f"{curve_x[0].type.name} ({curve_x[0].type.unit})")
    plt.ylabel(f"{curve_y[0].type.name} ({curve_y[0].type.unit})")
    plot_metadata(x_vals, y_vals, curve_x, device_type)
    plot_metadata(x_vals, y_vals, curve_y, device_type)


def plot_metadata(x_vals: list[float], y_vals: list[float], curve: tuple[mscript.MScriptVar], device_type):
    prev_range = None
    for x_val, y_val, p in zip(x_vals, y_vals, curve):
        if 'cr' in p.metadata:
            # Add annotation to data points to indicate change in range metadata.
            meas_range = p.metadata['cr']
            if meas_range != prev_range:
                range_label = mscript.metadata_range_to_text(device_type, p.type, meas_range)
                plt.annotate(text=range_label, xy=(x_val, y_val), xycoords='data',
                             xytext=(-10, 20), textcoords='offset points',
                             arrowprops=dict(arrowstyle='-', facecolor='black'))
                prev_range = meas_range

        if 'status' in p.metadata:
            # Plot data point marker to indicate 'status' metadata.
            status = p.metadata['status']
            if status & 0x4:
                # Yellow down arrow as 'underload' marker.
                plt.plot(x_val, y_val, color='y', marker='v')
            if status & 0x8:
                # Yellow up arrow as 'overload warning' marker.
                plt.plot(x_val, y_val, color='y', marker='^')
            if status & 0x2:
                # Orange up arrow as 'overload' marker.
                plt.plot(x_val, y_val, color='tab:orange', marker='^')
            if status & 0x1:
                # Red cross as 'timing not met' marker.
                plt.plot(x_val, y_val, color='r', marker='x')


if __name__ == '__main__':
    main()
