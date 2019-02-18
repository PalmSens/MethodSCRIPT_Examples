/*
 ============================================================================
 Name        : PSComm.h
 Author      : 
 Version     :
 Copyright   :
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
 Description :
 * ----------------------------------------------------------------------------
 *  PSComm handles the communication with the EmStat pico.
 *	Only this file needs to be included in your software.
 *	To communicate with an EmStat, create a PSComm struct and call PSCommInit() on it.
 *	If communication with multiple EmStats is required, create a separate PSComm with different
 * 	communication ports (defined by the write_char_func and read_char_func) for each EmStat.
 *
 *	Once a PSComm struct has been successfully created, send the parameters for measurement
 *	by reading a script string/script file for a specific measurement type.
 *
 *	To receive data from the EmStat, call ReceivePackage() periodically.
 ============================================================================
 */

#ifndef PSComm_H
#define PSComm_H

//////////////////////////////////////////////////////////////////////////////
// Includes
//////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <stddef.h>
#include "PSCommon.h"

//////////////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////////////

#define VERSION_STR_LENGTH	28

const int OFFSET_VALUE = 0x8000000;

///
/// Whether the cell should be on or off.
///
typedef enum _CellOnOff
{
	CELL_ON	 = 0x03,
	CELL_OFF = 0x05,
} CellOnOff;

///
/// Current status, if there is a underload or overload that datapoint may be inaccurate.
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
/// Possible replies from the EmStat.
///
typedef enum _Reply
{
	REPLY_MEASURING = 'M',
	REPLY_MEASURE_DP	= 'P',
	REPLY_ENDOFMEASLOOP	= '*'
} Reply;

///
/// The communication object for one EmStat.
/// You can instantiate multiple PSComms if you have multiple EmStat picos,
/// but you will need write / read functions from separate (serial) ports to talk to them.
///
typedef struct _PSComm
{
	WriteCharFunc write_char_func;
	ReadCharFunc read_char_func;
	TimeMsFunc time_ms_func;
	uint16_t read_timeout_ms;
} PSComm;

///
/// Encapsulates the data packages received from the emstat pico,
/// for measurement package
///
typedef struct _MeasureData
{
	/// Potential in Volts
	float potential;
	/// Current in Current Range
	float current;
	/// Reading status
	char* status;
	/// Current range
	char* cr;
} MeasureData;

//////////////////////////////////////////////////////////////////////////////
// Normal Functions
//
// These functions are used during normal operation.
//////////////////////////////////////////////////////////////////////////////

///
/// Initialize the PSComm object.
///
/// espComm:			The PSComm data struct.
/// read_timeout_ms: 	Timeout for all reads performed by PSComm.
/// write_char_func: 	Function pointer to the write function this PSComm should use.
/// read_char_func: 	Function pointer to the read function this PSComm should use.
/// time_ms_func:		Function pointer to a function that returns a time in ms.
///
/// Returns: CODE_OK if successful, otherwise CODE_NULL.
///
RetCode PSCommInit(PSComm* espComm, uint16_t read_timeout_ms,
	WriteCharFunc write_char_func, ReadCharFunc read_char_func, TimeMsFunc time_ms_func);

///
/// Wait for a package and parse it.
/// Currents are expressed in the Ampere, potentials are expressed in Volts.
///
/// espComm:	The PSComm data struct.
/// ret_data: 	The package received is parsed and put into this struct.
///
/// Returns: CODE_OK if successful, CODE_MEASUREMENT_DONE if measurement is completed.
RetCode ReceivePackage(PSComm* espComm, MeasureData* ret_data);

///
/// Parses a line of response and further calls to parse meta data values.
///
void ParseResponse(char *responseLine, MeasureData* ret_data);

///
/// Splits the input string in to tokens based on the delimiters set (delim) and stores the pointer to the successive token in *stringp.
/// This has to be performed repeatedly until end of string or until no further tokens are found.
///
/// Returns: the current token (char* ) found
char* strtokenize(char** stringp, const char* delim);

///
/// Parses a parameter and calls the function to parse meta data values if any.
///
void ParseParam(char* param, MeasureData* ret_data);

///
/// Retrieves the parameter value by parsing the input value and appending the SI uint prefix to it.
///
/// Returns: The actual parameter value in float (with its SI unit prefix)
float GetParameterValue(char* paramValue);

///
/// Parses the meta data values and calls the corresponding functions based on the meta data type (status, current range, noise)
///
void ParseMetaDataValues(char *metaDataParams, MeasureData* ret_data);

///
/// Parses the bytes corresponding to the status of the package(OK, Overload, Underload, Overload_warning)
///
char* GetReadingStatusFromPackage(char* metaDataStatus);

///
/// Parses the bytes corresponding to current range of the parameter
///
char* GetCurrentRangeFromPackage(char* metaDataCR);

///
/// Returns the double value corresponding to the input unit prefix char
///
const double GetUnitPrefixValue(char charPrefix);


//////////////////////////////////////////////////////////////////////////////
// Internal Communication Functions
//
// These functions are used by the SDK internally to communicate with the emstat pico.
// You probably don't need to use these directly, but they're here so you can if you want.
//////////////////////////////////////////////////////////////////////////////

///
/// Reads a character buffer using the supplied read_char_func
/// Returns: CODE_OK if successful, otherwise CODE_NULL.
///
RetCode ReadBuf(PSComm* espComm, char* buf);

///
/// Reads a character using the supplied read_char_func
/// Returns: CODE_OK if successful, otherwise CODE_TIMEOUT.
///
RetCode ReadChar(PSComm* espComm, char* c);

///
/// Writes a character using the supplied write_char_func
///
void WriteChar(PSComm* espComm, char c);

///
/// Writes a 0 terminated string using the supplied write_char_func
///
void WriteStr(PSComm* espComm, const char* buf);

#endif //PSComm_H
