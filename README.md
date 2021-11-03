[License](https://github.com/palmsens/emstatpico/blob/master/LICENSE)

# MethodSCRIPT 1.3 code examples
This repository contains the following code examples to use with the [EmStat Pico potentiostat module](http://www.emstatpico.com):

1. MethodSCRIPTExample_Android
1. MethodSCRIPTExample_Arduino
1. MethodSCRIPTExample_C
1. MethodSCRIPTExample_Python
1. MethodSCRIPTExample_iOS
1. MethodSCRIPTExamples_C#

The following examples support the EmStat4 as well as the EmStat Pico:

1. MethodSCRIPTExample_Arduino
1. MethodSCRIPTExample_C
1. MethodSCRIPTExample_Python

**Each folder contains a document with more information about how to use the example.**

##  MethodSCRIPTExample_Android
This project demonstrates basic communication with the EmStat Pico from an Android device. The examples show how to connect to the device **(using USB and Bluetooth)**, send a MethodSCRIPT to the device, run measurements on the device, read and parse the measurement data packages from the device using Java.

## MethodSCRIPTExample_Arduino
The arduino example MethodSCRIPTExample.ino demonstrates basic communication with a MethodSCRIPT device (EmStat4 or EmStat Pico) through Arduino MKR ZERO using the MethodSCRIPT SDK (C libraries). The example allows the user to start measurements from a Windows PC connected to the Arduino through USB.

## MethodSCRIPTExample_C
The example MethodSCRIPTExample.c found in the /MethodSCRIPTExample_C folder demonstrates basic communication with a MethodSCRIPT device (EmStat4 or EmStat Pico). The example allows the user to start measurements on the EmStat Pico from a Windows or Linux PC using a simple C program which makes use of the MethodSCRIPT SDK (C libraries). 

## MethodSCRIPTExample_Python
The example MSConsoleExample.py found in the /MethodSCRIPTExample_Python folder demonstrates basic communication with a MethodSCRIPT device (EmStat4 or EmStat Pico) using Python.
* The files `serial.py`, `mscript.py` and `instrument.py` contain a custom library with some commonly used functions for communication with MethodSCRIPT devices.
* The console_example.py example is a barebones example without a plotting option. It starts a measurement script and parses the measurement response into a CSV format.
* The plot_advanced_swv.py example demonstrates the common electrochemical technique "Square Wave Voltammetry". It also showcases some advanced plotting options such as plotting multiple curves on the same axis.
* The plot_cv.py example demonstrates the common electrochemical technique "Cyclic Voltammetry" and plots the resulting voltammogram. 
* The plot_eis.py example demonstrates the Electrochemical Impedance Spectroscopy technique and plots the resulting Nyquist and Bode plots.

## MethodSCRIPTExample_iOS
The example in the ‘MethodSCRIPTExamples_iOS’ folder demonstrates basic communication with the EmStat Pico from an iOS device using Bluetooth Low Energy (BLE). The example shows how to connect to an EmStat Pico, send a MethodSCRIPT and read and parse the measurement data packages using Apple’s programming language Swift. The UI was built with SwiftUI.

## MethodSCRIPTExamples_C#
The examples in the /MethodSCRIPTExamples_C# folder demonstrate basic communication with the EmStat Pico from a windows PC using C#. The examples show how to connect to the device, send MethodSCRIPTs to the device, run measurements on the device, read and parse the measurement data packages from the device and using simple plot objects to plot the data.

Included:

**Example 1: Basic Console Example (MSConsoleExample)**
This example demonstrates how to implement USB serial communication to
* Establish a connection with the device
* Write a MethodSCRIPT to the device
* Read and parse measurement data packages from the device
This does not include error handling, method validation etc.

**Example 2: Plot Example (MSPlotExample)**
In addition to the basic communications as in the above example, this plot example demonstrates how to implement the plot object (using the OxyPlot library for windows forms).

**Example 3: EIS Console Example (MSEISExample)**
This console example demonstrates sending, receiving and parsing data for a simple EIS measurement.

**Example 4: EIS Plot Example (MSEISPlotExample)**
This example demonstrates the implementation of OxyPlot to show the EIS measurement response on Nyquist and Bode plots.

# Support #
The EmStat Pico module is a product from PalmSens BV and Analog Devices Inc.
All support questions should be posted in our [EngineerZone forums](https://ez.analog.com/partnerzone/palmsens/). 

