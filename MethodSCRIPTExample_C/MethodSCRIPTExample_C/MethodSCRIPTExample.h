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
#include "MethodSCRIPTcomm/MSCommon.h"


//////////////////////////////////////////////////////////////////////////////
// Constants and defines
//////////////////////////////////////////////////////////////////////////////
#define CMD_VERSION_STRING "t\n"


// Serial port configuration
#ifdef __WIN32	// Windows system
	#define SERIAL_PORT_NAME "\\\\.\\COM8"							// The name of the port - to be changed, by looking up the device manager
	// Note port number to start with "\\\\.\\" to allow for any port number in Windows.
#else			// Linux. the port name to be changed. Can be found using "dmesg | grep FTDI" in the terminal
	#define SERIAL_PORT_NAME "/dev/ttyUSB0"
#endif
#define BAUD_RATE 230400										   // The baud rate for EmStat Pico

// Maximum number of characters that the EmStat Pico can receive in one line
#define MS_MAX_LINECHARS	128


//
// This file is shared between Windows an Linux examples, so we need to add the missing links.
//
#ifdef __WIN32 // Windows system
	#include <windows.h>
	#define SET_SEPARATOR_FOR_MS_EXCEL 1 // Set to 0 if NOT using Excel to read the CSV file
#else // Linux system
	#define Sleep(time) usleep(time * 1000)
	#define SET_SEPARATOR_FOR_MS_EXCEL 0
#endif

#endif //ESPICOCODEEXAMPLE_H


