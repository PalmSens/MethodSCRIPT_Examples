#!/usr/bin/env python3
"""
PalmSens MethodSCRIPT example: Electrochemical Impedance Spectroscopy (EIS)

This example showcases how to run an Electrochemical Impedance Spectroscopy
(EIS) measurement on a MethodSCRIPT capable PalmSens instrument, such as the
EmStat Pico.

The following features are demonstrated in this example:
  - Connecting to the PalmSens instrument using the serial port.
  - Running an Electrochemical Impedance Spectroscopy (EIS) measurement.
  - Receiving and interpreting the measurement data from the device.
  - Plotting the measurement data.

-------------------------------------------------------------------------------
Copyright (c) 2020 PalmSens BV
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
import os.path
import sys

# Third-party imports
import matplotlib.pyplot as plt
import numpy as np

# Local imports
import palmsens.instrument
import palmsens.mscript
import palmsens.serial


###############################################################################
# Start of configuration
###############################################################################

# COM port of the device (None = auto detect).
DEVICE_PORT = None

# Location of MethodSCRIPT file to use.
MSCRIPT_FILE_PATH = 'scripts/example_eis.mscr'

# Location of output files. Directory will be created if it does not exist.
OUTPUT_PATH = 'output'

# Plot colors.
AX1_COLOR = 'tab:red'   # Color for impedance (Z)
AX2_COLOR = 'tab:blue'  # Color for phase

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

    port = DEVICE_PORT
    if port is None:
        port = palmsens.serial.auto_detect_port()

    # Create and open serial connection to the device.
    with palmsens.serial.Serial(port, 1) as comm:
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
    result_file_name = datetime.datetime.now().strftime('ms_plot_eis_%Y%m%d-%H%M%S.txt')
    result_file_path = os.path.join(OUTPUT_PATH, result_file_name)
    with open(result_file_path, 'wt', encoding='ascii') as file:
        file.writelines(result_lines)

    # Parse the result.
    curves = palmsens.mscript.parse_result_lines(result_lines)

    # Log the results.
    for curve in curves:
        for package in curve:
            LOG.info([str(value) for value in package])

    # Get the applied frequencies.
    applied_frequency = palmsens.mscript.get_values_by_column(curves, 0)
    # Get the measured real part of the complex impedance.
    measured_z_real = palmsens.mscript.get_values_by_column(curves, 1)
    # Get the measured imaginary part of the complex impedance.
    measured_z_imag = palmsens.mscript.get_values_by_column(curves, 2)

    # Calculate Z and phase.
    # Invert the imaginary part for the electrochemist convention.
    measured_z_imag = -measured_z_imag
    # Compose the complex impedance.
    z_complex = measured_z_real + 1j * measured_z_imag
    # Get the phase from the complex impedance in degrees.
    z_phase = np.angle(z_complex, deg=True)
    # Get the impedance value.
    z = np.abs(z_complex)

    # Plot the results.
    # Show the Nyquist plot as figure 1.
    plt.figure(1)
    plt.plot(measured_z_real, measured_z_imag)
    plt.title('Nyquist plot')
    plt.axis('equal')
    plt.grid()
    plt.xlabel("Z'")
    plt.ylabel("-Z''")

    # Show the Bode plot as dual y axis (sharing the same x axis).
    fig, ax1 = plt.subplots()
    ax2 = ax1.twinx()

    ax1.set_xlabel('Frequency (Hz)')
    ax1.set_ylabel('Z', color=AX1_COLOR)
    # Make x axis logarithmic.
    ax1.semilogx(applied_frequency, z, color=AX1_COLOR)
    # Show ticks.
    ax1.tick_params(axis='y', labelcolor=AX1_COLOR)
    # Turn on the minor ticks, which are required for the minor grid.
    ax1.minorticks_on()
    # Customize the major grid.
    ax1.grid(which='major', linestyle='-', linewidth='0.1', color='black')

    # We already set the x label with ax1.
    ax2.set_ylabel('-Phase (degrees)', color=AX2_COLOR)
    ax2.semilogx(applied_frequency, z_phase, color=AX2_COLOR)
    ax2.tick_params(axis='y', labelcolor=AX2_COLOR)

    # Uncomment the following line if some of the labels are clipped.
    # fig.tight_layout()
    plt.grid(True, which='both')
    plt.title('Bode plot')
    plt.show()


if __name__ == '__main__':
    main()
