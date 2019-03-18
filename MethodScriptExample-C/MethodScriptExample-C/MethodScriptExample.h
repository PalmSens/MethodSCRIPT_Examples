/* ----------------------------------------------------------------------------
 *         PalmSens SDK //TODO: name inconsistent with .c file
 * ----------------------------------------------------------------------------
 * Copyright (c) 2016, PalmSens BV
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

#include <windows.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "MethodScriptComm/MSComm.h"

//////////////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////////////
const char* CMD_VERSION_STRING = "t\n";


//////////////////////////////////////////////////////////////////////////////
// Normal Functions
//
// These functions are used during normal operation.
//////////////////////////////////////////////////////////////////////////////

///
/// Opens the serial port to which Emstat pico is connected.
///
/// Returns: 1 on successful connection, 0 in case of failure.
///
int OpenSerialPort();

///
/// Verifies if the connected device is EmStat Pico.
///
/// Returns: 1 if EmStat Pico is detected, 0 in case of failure.
///
BOOL VerifyEmStatPico();

///
/// Reads a line from the script file and writes it to the EmStat Pico
///
/// Returns: 1 if data is read from file and written successfully to the device, 0 in case of failure.
int SendScriptFile(char* fileName);

///
/// Writes the input character to the device
///
/// Returns: 1 if data is written successfully, 0 in case of failure.
int WriteToDevice(char c);

///
/// Returns a character read from the EmStat Pico
///
int ReadFromDevice();

///
/// Prints the parsed values on the console
///
int DisplayResults(RetCode code);

#endif //ESPICOCODEEXAMPLE_H




