[License](https://github.com/palmsens/emstatpico/blob/master/LICENSE)

# Implementing MethodSCRIPT 1.8 - code examples
This repository contains programming examples to use with MethodSCRIPT capable PalmSens instruments. The following devices are tested to work with these examples: The [EmStat Pico potentiostat module](https://www.emstatpico.com), the [EmStat4M potentiostat module](https://www.palmsens.com/emstat4m) and the [Nexus benchtop potentiostat](https://www.palmsens.com/nexus).

Examples for the following programming languages are available:

1. Example_Arduino
1. Example_C
1. Example_Python

**Each folder contains a document with more information about how to use the example.**

The _/MethodSCRIPTs_ folder is filled with a large set of example MethodSCRIPT files to be used with supported devices using PSTrace or one of the programming examples mentioned above. These examples serve as a showcase for specific MethodSCRIPT commands and as templates that can be edited to fit your application.

## Example_Arduino
The arduino example Example.ino demonstrates basic communication with a MethodSCRIPT device (EmStat4 or EmStat Pico) through Arduino MKR ZERO using C libraries for communication. The example allows the user to start measurements from a computer connected to the Arduino through USB.

## Example_C
The example Example.c found in the _/Example_C_ folder demonstrates basic communication with a MethodSCRIPT device (EmStat4, EmStat Pico or Nexus). The example allows the user to start measurements on the device from a Windows or Linux PC using a simple C program which makes use of C source files. 

## Example_Python
The example MSConsoleExample.py found in the _/Example_Python_ folder demonstrates basic communication with a MethodSCRIPT device (EmStat4, EmStat Pico or Nexus) using Python.
* The files `serial.py`, `mscript.py` and `instrument.py` contain a custom library with some commonly used functions for communication with MethodSCRIPT devices.
* The console_example.py example is a barebones example without a plotting option. It starts a measurement script and parses the measurement response into a CSV format.
* The plot_advanced_swv.py example demonstrates the common electrochemical technique "Square Wave Voltammetry". It also showcases some advanced plotting options such as plotting multiple curves on the same axis.
* The plot_cv.py example demonstrates the common electrochemical technique "Cyclic Voltammetry" and plots the resulting voltammogram. 
* The plot_eis.py example demonstrates the Electrochemical Impedance Spectroscopy technique and plots the resulting Nyquist and Bode plots.

# Support #
The Emstat4M and Nexus are products from PalmSens BV.

The EmStat Pico module is a product from PalmSens BV and Analog Devices Inc.

More information about developing with our devices can be found in our [Documentation for Developers Website](https://dev.palmsens.com/site).

For support you can contact us at [support@palmsens.com](mailto:support@palmsens.com?SUBJECT=Support%20for%20MethodSCRIPT%20Examples&BODY=Please%20give%20a%20detailed%20description%20about%20your%20problem)
