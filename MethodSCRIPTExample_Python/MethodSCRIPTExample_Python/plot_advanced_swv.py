#!/usr/bin/env python3
"""
PalmSens MethodSCRIPT example: advanced SWV measurement and plotting

This example showcases how to run two Square Wave Voltammetry (SWV)
measurements using an EmStat Pico instrument, and then plot the results for
easy verification.

To run this example, connect the EmStat Pico to the RedOx Circuit (WE A) on
the PalmSens Dummy Cell.

The following features are demonstrated in this example:
  - Connecting to the EmStat Pico using the serial port.
  - Running two consecutive Square Wave Voltammetry (SWV) measurements.
  - Receiving and interpreting the measurement data from the device.
  - Plotting the measurement data from both measurements in a plot.
    Specifically, multiple curves are drawn in the same figure against a
    common x axis.
  - Exporting the measurement data to a file in CSV format.

Although this example demonstrates how to plot the SWV measurement data from
an EmStat Pico device, the principles could be applied just as well for other
measurement types using any MethodSCRIPT capable PalmSens instrument. The code
can easily be modified for other (similar) types of measurements.

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
import csv
import datetime
import logging
import os.path
import sys

# Third-party imports
import matplotlib.pyplot as plt

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
MSCRIPT_FILE_PATH_ES4 = 'scripts/example_advanced_swv_es4.mscr'
MSCRIPT_FILE_PATH_ESPICO = 'scripts/example_advanced_swv_espico.mscr'

# Location of output files. Directory will be created if it does not exist.
OUTPUT_PATH = 'output'

# In this example, columns refer to the separate "pck_add" entries in each
# MethodSCRIPT data package. For example, in the used script there are
# four columns per data package:
#   pck_start
#   pck_add p
#   pck_add c
#   pck_add f
#   pck_add r
#   pck_end
# Here, the indices 0/1/2/3 refer to the variables p/c/f/r/ respectively.
# The following variables define which columns (i.e. variables) to plot, and
# how they are named in the figure.

# Column names.
COLUMN_NAMES = ['Potential', 'Current', 'Forward Current', 'Reverse Current']
# Index of column to put on the x axis.
XAXIS_COLUMN_INDEX = 0
# Indices of columns to put on the y axis. The variables must be same type.
YAXIS_COLUMN_INDICES = [1, 2, 3]

###############################################################################
# End of configuration
###############################################################################


LOG = logging.getLogger(__name__)


def write_curves_to_csv(file, curves):
    """Write the curves to file in CSV format.

    `file` must be a file-like object in text mode with newlines translation
    disabled.

    The header row is based on the first row of the first curve. It is assumed
    that all rows in all curves have the same data types.
    """
    # NOTE: Although the extension is CSV, which stands for Comma Separated
    # Values, a semicolon (';') is used as delimiter. This is done to be
    # compatible with MS Excel, which may use a comma (',') as decimal
    # separator in some regions, depending on regional settings on the
    # computer. If you use anoter program to read the CSV files, you may need
    # to change this. The CSV writer can be configured differently to support
    # a different format. See
    # https://docs.python.org/3/library/csv.html#csv.writer for all options.
    # NOTE: The following line writes a Microsoft Excel specific header line to
    # the CSV file, to tell it we use a semicolon as delimiter. If you don't
    # use Excel to read the CSV file, you might want to remove this line.
    file.write('sep=;\n')
    writer = csv.writer(file, delimiter=';')
    for curve in curves:
        # Write header row.
        writer.writerow(['%s [%s]' % (value.type.name, value.type.unit) for value in curve[0]])
        # Write data rows.
        for package in curve:
            writer.writerow([value.value for value in package])


def main():
    """Run the example."""
    # Configure the logging.
    logging.basicConfig(level=logging.DEBUG, format='[%(module)s] %(message)s',
                        stream=sys.stdout)
    # Uncomment the following line to reduce the log level of our library.
    # logging.getLogger('palmsens').setLevel(logging.INFO)
    # Disable excessive logging from matplotlib.
    logging.getLogger('matplotlib').setLevel(logging.INFO)

    # Determine unique name for plot and files.
    base_name = datetime.datetime.now().strftime('ms_plot_swv_%Y%m%d-%H%M%S')
    # Base path contains directory and base name. Extension is appended later.
    base_path = os.path.join(OUTPUT_PATH, base_name)

    port = DEVICE_PORT
    if port is None:
        port = palmsens.serial.auto_detect_port()

    # Create and open serial connection to the device.
    with palmsens.serial.Serial(port, 1) as comm:
        device = palmsens.instrument.Instrument(comm)
        device_type = device.get_device_type()
        LOG.info('Connected to %s.', device_type)

        if device_type == palmsens.instrument.DeviceType.EMSTAT_PICO:
            mscript_file_path = MSCRIPT_FILE_PATH_ESPICO
        elif 'EmStat4' in device_type:
            mscript_file_path = MSCRIPT_FILE_PATH_ES4
        else:
            LOG.error('No SWV script for this device found.')
            return

        # Read and send the MethodSCRIPT file.
        LOG.info('Sending MethodSCRIPT.')
        device.send_script(mscript_file_path)

        # Read the result lines.
        LOG.info('Waiting for results.')
        result_lines = device.readlines_until_end()

    # Store results in file.
    os.makedirs(OUTPUT_PATH, exist_ok=True)
    with open(base_path + '.txt', 'wt', encoding='ascii') as file:
        file.writelines(result_lines)

    # Parse the result.
    curves = palmsens.mscript.parse_result_lines(result_lines)

    # Store results in CSV format.
    with open(base_path + '.csv', 'wt', newline='', encoding='ascii') as file:
        write_curves_to_csv(file, curves)

    # Create and configure a plot for the results.
    plt.figure()
    plt.title(base_name)
    # Put specified column of the first curve on x axis.
    xvar = curves[0][0][XAXIS_COLUMN_INDEX]
    plt.xlabel('%s [%s]' % (xvar.type.name, xvar.type.unit))
    # Put specified column of the first curve on y axis.
    yvar = curves[0][0][YAXIS_COLUMN_INDICES[0]]
    plt.ylabel('%s [%s]' % (yvar.type.name, yvar.type.unit))
    plt.grid(b=True, which='major')
    plt.grid(b=True, which='minor', color='b', linestyle='-', alpha=0.2)
    plt.minorticks_on()

    # Loop through all curves and plot them.
    for icurve in range(len(curves)):
        # Get xaxis column for this curve.
        xvalues = palmsens.mscript.get_values_by_column(
            curves, XAXIS_COLUMN_INDEX, icurve)
        # Loop through all y axis columns and plot one curve per column.
        for yaxis_column_index in YAXIS_COLUMN_INDICES:
            yvalues = palmsens.mscript.get_values_by_column(
                curves, yaxis_column_index, icurve)

            # Ignore invalid columns.
            if curves[icurve][0][yaxis_column_index].type != yvar.type:
                continue

            # Make plot label.
            # If there are multiple curves, add the curve index to the label.
            if len(curves) > 1:
                label = '%s vs %s %d' % (COLUMN_NAMES[yaxis_column_index],
                                         COLUMN_NAMES[XAXIS_COLUMN_INDEX], icurve)
            else:
                label = '%s vs %s' % (COLUMN_NAMES[yaxis_column_index],
                                      COLUMN_NAMES[XAXIS_COLUMN_INDEX])

            # Plot the curve y axis against the global x axis.
            plt.plot(xvalues, yvalues, label=label)

    # Generate legend.
    plt.legend()

    plt.savefig(base_path + '.png')
    plt.show()


if __name__ == '__main__':
    main()
