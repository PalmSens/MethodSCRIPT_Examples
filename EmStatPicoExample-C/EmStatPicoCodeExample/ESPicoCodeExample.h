/* ----------------------------------------------------------------------------
 *         PalmSens EmStat SDK
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
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stddef.h>


//////////////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////////////
const int OFFSET_VAL = 0x8000000;

//////////////////////////////////////////////////////////////////////////////
// Types
//////////////////////////////////////////////////////////////////////////////

///
/// Function return codes.
///
typedef enum _RetCode
{
	CODE_MEASURING			= 2,
	CODE_MEASUREMENT_DONE	= 1,
	CODE_OK 				= 0,
	CODE_NULL 				= -1,
	CODE_TIMEOUT			= -2,
	CODE_OUT_OF_RANGE		= -3,
	CODE_UNEXPECTED_DATA	= -4,
	CODE_NOT_IMPLEMENTED 	= -5,
} RetCode;


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
/// Reads a line from the script file and writes it to the Emstat pico.
///
/// Returns: 1 if data is read from file and written successfully to the device, 0 in case of failure.
int ReadScriptFile(char* fileName);

///
/// Writes a 0 terminated string using the supplied write_char_func
///
/// Returns: 1 if data is written successfully, 0 in case of failure.
int WriteToDevice();

///
/// Reads a character from the device until a new line is found and forms a line of response.
///
/// Returns: CODE_OK if any character/line is read, CODE_NULL if nothing's read.
RetCode ReadBufferLine(char *bufferLine);

///
/// Reads the measurement response from the device and calls the parse function to get the parameter values.
///
void ReadMeasurementResponse();

///
/// Parses a line of response and splits the parameters to be parsed further to get meta data values.
///
void ParseResponse(char *responseLine);

///
/// Splits the input string in to tokens based on the delimiters set (delim) and stores the pointer to the successive token in *stringp.
/// This has to be performed repeatedly until end of string or until no further tokens are found.
///
/// Returns: the current token (char* ) found
char* strtokenize(char** stringp, const char* delim);

///
/// Parses a parameter and calls the function to parse meta data values if any.
///
void ParseParam(char* param);

///
/// Retrieves the parameter value by parsing the input value and appending the SI uint prefix to it.
///
/// Returns: The actual parameter value in float (with its SI unit prefix)
float GetParameterValue(char* paramValue);

///
/// Parses the meta data values and calls the corresponding functions based on the meta data type (status, current range, noise)
///
void ParseMetaDataVals(char* metaDataParams);

///
/// Parses the bytes corresponding to the status of the package(OK, Overload, Underload, Overload_warning)
///
void GetReadingStatusFromPackage(char* metaDatastatus);

///
/// Parses the bytes corresponding to current range of the parameter
///
void GetCurrentRangeFromPackage(char* metaDataCR);

///
/// Returns the double value corresponding to the input unit prefix char
///
const double GetUnitPrefixVal(char charPrefix);


