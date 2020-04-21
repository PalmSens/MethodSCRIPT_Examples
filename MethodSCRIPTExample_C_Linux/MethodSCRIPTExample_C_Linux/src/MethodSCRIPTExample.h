/* ----------------------------------------------------------------------------
 *         PalmSens MethodSCRIPT SDK
 * ----------------------------------------------------------------------------
 * Copyright (c) 2019-2020, PalmSens BV
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * PalmSens's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY PALMSENS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL PALMSENS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

//////////////////////////////////////////////////////////////////////////////
// Includes
//////////////////////////////////////////////////////////////////////////////

#ifndef ESPICOCODEEXAMPLE_H
#define ESPICOCODEEXAMPLE_H

#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>

#include "MethodSCRIPTcomm/MSComm.h"

//////////////////////////////////////////////////////////////////////////////
// Constants and defines
//////////////////////////////////////////////////////////////////////////////
#define CMD_VERSION_STRING "t\n"

// Serial port configuration
#ifdef WIN32	// Windows
	#define PORT_NAME "\\\\.\\COM12"								// The name of the port - to be changed, by looking up the device manager
	// Note port number to start with "\\\\.\\" to allow for any port number in Windows.
#else			// Linux. the port name to be changed. Can be found using "dmesg | grep FTDI" in the terminal
	#define PORT_NAME "/dev/ttyUSB0"
#endif
#define BAUD_RATE 230400										   // The baud rate for EmStat Pico

#define SUCCESS 1
#define FAILURE 0

//////////////////////////////////////////////////////////////////////////////
// Normal Functions
//
// These functions are used during normal operation.
//////////////////////////////////////////////////////////////////////////////


///
/// Verifies if the connected device is EmStat Pico.
///
/// Returns: 1 if EmStat Pico is detected, 0 in case of failure.
///
bool VerifyEmStatPico();


///
/// Reads a line from the script file and writes it to the EmStat Pico
///
/// Returns: 1 if data is read from file and written successfully to the device, 0 in case of failure.
///
int SendScriptFile(char* fileName);


/// Prints the parsed values on the console or (-1) in case of an error.
///
int DisplayResults(RetCode code, MeasureData result, int *nDataPoints);


///
/// Write a datapoint result to a CSV File
///
void ResultsToCsv(RetCode code, MeasureData result, int nDataPoints);


///
/// Opens a CSV File
///
void OpenCSVFile(const char *pFilename,FILE **fp);


///
/// Write data to (append) a CSV File
///
void WriteDataToCSVFile(FILE *fp, MeasureData resultdata, int nDataPoints);


///
/// Write the header line to the CSV file
///
void WriteHeaderToCSVFile(FILE *fp);


#endif //ESPICOCODEEXAMPLE_H


