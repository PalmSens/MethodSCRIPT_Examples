/* ----------------------------------------------------------------------------
 *         PalmSens MethodSCRIPT SDK
 * ----------------------------------------------------------------------------
 ============================================================================
 Name        : MSComm.h
 Copyright   :
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
 Description :
 * ----------------------------------------------------------------------------
 *  MSComm handles the communication with the EmStat pico.
 *	Only this file needs to be included in your software.
 *	To communicate with an EmStat Pico, create a MSComm struct and call MSCommInit() on it.
 *	If communication with multiple EmStat Picos is required, create a separate MSComm with different
 * 	communication ports (defined by the write_char_func and read_char_func) for each EmStat Pico.
 *
 *	Once a MSComm struct has been successfully created, send the parameters for measurement
 *	by reading a script string/script file for a specific measurement type.
 *
 *	To receive data from the EmStat Pico, call ReceivePackage().
 ============================================================================
 */

#ifndef MSComm_H
#define MSComm_H

//////////////////////////////////////////////////////////////////////////////
// Includes
//////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MSCommon.h"

//////////////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////////////

#define VERSION_STR_LENGTH	28

///
/// Whether the cell should be on or off.
///
typedef enum _CellOnOff
{
	CELL_ON	 = 0x03,
	CELL_OFF = 0x05,
} CellOnOff;

///
/// Current status, if there is a underload or overload
///
typedef enum _Status
{
	///Status OK
	STATUS_OK = 0x0,
	///Current overload
	STATUS_OVERLOAD = 0x2,
	///Current underload
	STATUS_UNDERLOAD = 0x4,
	///Overload warning
	STATUS_OVERLOAD_WARNING = 0x8
} Status;

///
/// Possible replies from the EmStat Pico
///
typedef enum _Reply
{
	REPLY_VERSION_RESPONSE 	= 't',
	REPLY_MEASURING 		= 'M',
	REPLY_MEASURE_DP		= 'P',
	REPLY_ENDOFMEASLOOP		= '*'
} Reply;

///
/// The communication object for one EmStat Pico
/// You can instantiate multiple MSComms if you have multiple EmStat Picos,
/// but you will need write / read functions from separate (serial) ports to communicate with them
///
typedef struct _MSComm
{
	WriteCharFunc writeCharFunc;
	ReadCharFunc readCharFunc;
} MSComm;

///
/// Encapsulates the data packages received from the EmStat Pico for measurement package
///
typedef struct _MeasureData
{
	/// Potential in Volts
	float potential;
	/// Current in Ampere
	float current;
	/// Real part of complex impedance
	float zreal;
	/// Imaginary part of complex impedance
	float zimag;
	/// Applied frewquency
	float frequency;

	/// Reading status
	const char* status;
	/// Current range
	const char* cr;
} MeasureData;

//////////////////////////////////////////////////////////////////////////////
// Normal Functions
//
// These functions are used during normal operation.
//////////////////////////////////////////////////////////////////////////////

///
/// Initialises the MSComm object
///
/// MSComm:				The MSComm data struct
/// write_char_func: 	Function pointer to the write function this MSComm should use
/// read_char_func: 	Function pointer to the read function this MSComm should use
///
/// Returns: CODE_OK if successful, otherwise CODE_NULL.
///
RetCode MSCommInit(MSComm* MSComm,	WriteCharFunc write_char_func, ReadCharFunc read_char_func);

///
/// Receives a package and parses it
/// Currents are expressed in the Ampere, potentials are expressed in Volts
///
/// MSComm:		The MSComm data struct
/// retData: 	The package received is parsed and stored in this struct
///
/// Returns: CODE_OK if successful, CODE_MEASUREMENT_DONE if measurement is completed
///
RetCode ReceivePackage(MSComm* MSComm, MeasureData* retData);

///
/// Parses a line of response and passes the meta data values for further parsing
///
/// responseLine: The line of response to be parsed
/// retData: 	  The struct in which the parsed values are stored
///
void ParseResponse(char *responseLine, MeasureData* retData);

///
/// Splits the input string in to tokens based on the delimiters set (delim) and stores the pointer to the successive token in *stringp
/// This has to be performed repeatedly until end of string or until no further tokens are found
///
/// stringp: The pointer to the next string token
/// delim:   The array containing a set of delimiters
///
/// Returns: The current token (char* ) found
///
char* strtokenize(char** stringp, const char* delim);

///
/// Fetches the string to be displayed for the input status
///
/// status: The enum Status whose string is to be fetched
///
/// Returns: The corresponding string for the input enum status
///
const char* StatusToString(Status status);

///
/// Parses a parameter and calls the function to parse meta data values if any
///
/// param: 	 The parameter value to be parsed
/// retData: The struct in which the parsed values are stored
///
void ParseParam(char* param, MeasureData* retData);

///
/// Retrieves the parameter value by parsing the input value and appending the SI unit prefix to it
///
/// Returns: The actual parameter value in float (with its SI unit prefix)
///
float GetParameterValue(char* paramValue);

///
/// Parses the meta data values and calls the corresponding functions based on the meta data type (status, current range, noise)
///
/// metaDataParams: The meta data parameter values to be parsed
/// retData: 		The struct in which the parsed values are stored
///
void ParseMetaDataValues(char *metaDataParams, MeasureData* retData);

///
/// Parses the bytes corresponding to the status of the package(OK, Overload, Underload, Overload_warning)
///
/// metaDataStatus: The meta data status value to be parsed
///
/// Returns: A string corresponding to the parsed status

const char* GetReadingStatusFromPackage(char* metaDataStatus);

///
/// Parses the bytes corresponding to current range of the parameter
///
/// metaDataCR: The meta data current range value to be parsed
///
/// Returns: A string corresponding to the parsed current range value
///
const char* GetCurrentRangeFromPackage(char* metaDataCR);

///
/// Returns the double value corresponding to the input unit prefix char
///
const double GetUnitPrefixValue(char charPrefix);


//////////////////////////////////////////////////////////////////////////////
// Internal Communication Functions
//
// These functions are used by the SDK internally to communicate with the EmStat Pico.
// You probably don't need to use these directly, but they're here so you can if you want.
//////////////////////////////////////////////////////////////////////////////

///
/// Reads a character buffer using the supplied read_char_func
///
/// MSComm:	The MSComm data struct
/// buf:	The buffer in which the response is stored
///
/// Returns: CODE_OK if successful, otherwise CODE_NULL.
///
RetCode ReadBuf(MSComm* MSComm, char* buf);

///
/// Reads a character using the supplied read_char_func
///
/// MSComm:	The MSComm data struct
/// c:		The character read is stored in this variable
///
/// Returns: CODE_OK if successful, otherwise CODE_TIMEOUT.
///
RetCode ReadChar(MSComm* MSComm, char* c);

///
/// Writes a character using the supplied write_char_func
///
/// MSComm:	The MSComm data struct
/// c:		The character to be written
///
void WriteChar(MSComm* MSComm, char c);

///
/// Writes a 0 terminated string using the supplied write_char_func
///
/// MSComm:	The MSComm data struct
/// buf:	The data to be written
///
void WriteStr(MSComm* MSComm, const char* buf);

#endif //MSComm_H
