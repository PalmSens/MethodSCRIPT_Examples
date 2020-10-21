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
 * MSComm handles the communication with the EmStat Pico.
 *	Only this file needs to be included in your software.
 *	To communicate with an EmStat Pico, create a MSComm struct and call MSCommInit() on it.
 *	If communication with multiple EmStat Picos is required, create a separate MSComm with different
 * 	communication ports (defined by the write_char_func and read_char_func) for each EmStat Pico.
 *
 *	Once a MSComm struct has been successfully created, send the parameters for measurement
 *	by reading a script string/script file for a specific measurement type.
 *
 *	To receive data from the EmStat Pico, call ReceivePackage().
 *
 *	This library support MethodSCRIPT output packages with a fixed maximum number of subpackages.
 *	The maximum number is defined by `MSCR_SUBPACKAGES_PER_LINE` and statically allocated in the struct `MscrPackage`.
 *
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
// Constants and macros
//////////////////////////////////////////////////////////////////////////////


#define VERSION_STR_LENGTH	28

#define MSCR_SUBPACKAGES_PER_LINE	100


#define VARTYPE_TO_UINT8(ch1, ch2) (((ch1)-'a') * 26 + (ch2 - 'a'))
// Converts a MethodSCRIPT `variable type` string to an integer
// For example "aa" = 0, "ab = 1, "ba" = 26 and "zz" = 675)
#define MSCR_STR_TO_VT(str) (VARTYPE_TO_UINT8(str[0], str[1]))

//NOTE: Stored in uint8, so highest possible var type is 255, which is "jv"
typedef enum _VarType
{
	//Measured types
	//'a' category is potential
	MSCR_VT_UNKNOWN          	= VARTYPE_TO_UINT8('a', 'a'),    //Value is unset
	MSCR_VT_POTENTIAL	   		= VARTYPE_TO_UINT8('a', 'b'),		//WE / SE vs RE
	MSCR_VT_POTENTIAL_CE      	= VARTYPE_TO_UINT8('a', 'c'), 	//CE vs GND
	MSCR_VT_POTENTIAL_SE      	= VARTYPE_TO_UINT8('a', 'd'),		//SE vs GND
	MSCR_VT_POTENTIAL_RE      	= VARTYPE_TO_UINT8('a', 'e'),		//RE vs GND
	//MSCR_VT_POTENTIAL_WE      	= VARTYPE_TO_UINT8('a', 'f'),		//WE vs GND //Not sure this makes sense?
	MSCR_VT_POTENTIAL_WE_VS_CE	= VARTYPE_TO_UINT8('a', 'g'),		//WE / SE vs CE

	MSCR_VT_POTENTIAL_AIN0 		= VARTYPE_TO_UINT8('a', 's'),
	MSCR_VT_POTENTIAL_AIN1 		= VARTYPE_TO_UINT8('a', 't'),
	MSCR_VT_POTENTIAL_AIN2 		= VARTYPE_TO_UINT8('a', 'u'),
	MSCR_VT_POTENTIAL_AIN3 		= VARTYPE_TO_UINT8('a', 'v'),
	MSCR_VT_POTENTIAL_AIN4		= VARTYPE_TO_UINT8('a', 'w'),
	MSCR_VT_POTENTIAL_AIN5 		= VARTYPE_TO_UINT8('a', 'x'),
	MSCR_VT_POTENTIAL_AIN6 		= VARTYPE_TO_UINT8('a', 'y'),
	MSCR_VT_POTENTIAL_AIN7 		= VARTYPE_TO_UINT8('a', 'z'),

	//'b' category is current
	MSCR_VT_CURRENT       		= VARTYPE_TO_UINT8('b', 'a'), //WE current

	//'c' category is impedance
	MSCR_VT_PHASE             	= VARTYPE_TO_UINT8('c', 'a'),
	MSCR_VT_IMP               	= VARTYPE_TO_UINT8('c', 'b'),
	MSCR_VT_ZREAL             	= VARTYPE_TO_UINT8('c', 'c'),
	MSCR_VT_ZIMAG             	= VARTYPE_TO_UINT8('c', 'd'),

	//Applied types
	MSCR_VT_CELL_SET_POTENTIAL = VARTYPE_TO_UINT8('d', 'a'),
	MSCR_VT_CELL_SET_CURRENT   = VARTYPE_TO_UINT8('d', 'b'),
	MSCR_VT_CELL_SET_FREQUENCY = VARTYPE_TO_UINT8('d', 'c'),
	MSCR_VT_CELL_SET_AMPLITUDE = VARTYPE_TO_UINT8('d', 'd'),

	//Other types
	MSCR_VT_CHANNEL           	= VARTYPE_TO_UINT8('e', 'a'),
	MSCR_VT_TIME					= VARTYPE_TO_UINT8('e', 'b'),
	MSCR_VT_PIN_MSK				= VARTYPE_TO_UINT8('e', 'c'),

	//Internal (device specific) types for diagnostic purposes
	MSCR_VT_DEV_ADC_OFFSET 		= VARTYPE_TO_UINT8('g', 'a'),
	MSCR_VT_DEV_HS_EX 			= VARTYPE_TO_UINT8('g', 'b'),

	//Generic types (reserved but not implemented)
	MSCR_VT_CURRENT_GENERIC1  	= VARTYPE_TO_UINT8('h', 'a'),
	MSCR_VT_CURRENT_GENERIC2  	= VARTYPE_TO_UINT8('h', 'b'),
	MSCR_VT_CURRENT_GENERIC3  	= VARTYPE_TO_UINT8('h', 'c'),
	MSCR_VT_CURRENT_GENERIC4  	= VARTYPE_TO_UINT8('h', 'd'),
	MSCR_VT_POTENTIAL_GENERIC1 = VARTYPE_TO_UINT8('i', 'a'),
	MSCR_VT_POTENTIAL_GENERIC2 = VARTYPE_TO_UINT8('i', 'b'),
	MSCR_VT_POTENTIAL_GENERIC3 = VARTYPE_TO_UINT8('i', 'c'),
	MSCR_VT_POTENTIAL_GENERIC4 = VARTYPE_TO_UINT8('i', 'd'),
	MSCR_VT_MISC_GENERIC1      = VARTYPE_TO_UINT8('j', 'a'),
	MSCR_VT_MISC_GENERIC2     	= VARTYPE_TO_UINT8('j', 'b'),
	MSCR_VT_MISC_GENERIC3     	= VARTYPE_TO_UINT8('j', 'c'),
	MSCR_VT_MISC_GENERIC4     	= VARTYPE_TO_UINT8('j', 'd'),

	MSCR_VT_NONE 						= 255,
	MSCR_VT_CNT 						= 256,
} VarType;


///
/// Whether the cell should be on or off.
///
typedef enum _CellOnOff
{
	CELL_ON	 = 0x03,
	CELL_OFF = 0x05,
} CellOnOff;


///
/// Measurement Status, e.g. underload or overload
/// Uses a bitmask
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
	REPLY_NSCANS_START		= 'C',
	REPLY_NSCANS_DONE		= '-',
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
/// MethodSCRIPT subpackage metadata. This is part of a subpackage.
/// Metadata fields that are not given should be set to -1.
/// One subpackage can contain any combination of metadata fields
///
typedef struct _MscrMetadata {
	int status;          //
	int current_range;   //
} MscrMetadata;


///
/// Structure to store one MethodSCRIPT sub-package
///
typedef struct _MscrSubPackage {
	float        value;			 // The MethodSCRIPT value parsed from the sub-package string
	int          variable_type; // As converted by `MSCR_STR_TO_VT`
	MscrMetadata metadata;		 // The meta-data parsed from the sub-package string
} MscrSubPackage;


///
/// Structure to store one MethodScript package (line).
/// One line can contain N sub-packages.
///
typedef struct _MscrPackage {
	int nr_of_subpackages; // The number of currently used packages
	MscrSubPackage subpackages[MSCR_SUBPACKAGES_PER_LINE];
} MscrPackage;


//////////////////////////////////////////////////////////////////////////////
// Normal Functions
//
// These functions are used during normal operation.
//////////////////////////////////////////////////////////////////////////////

///
/// Initialises the MSComm object
///
/// parameters:
///   MSComm           - The MSComm data struct
///   write_char_func  - Function pointer to the write function this MSComm should use
///   read_char_func   - Function pointer to the read function this MSComm should use
///
/// Returns:
///   CODE_OK if successful, otherwise CODE_NULL.
///
RetCode MSCommInit(MSComm* MSComm,	WriteCharFunc write_char_func, ReadCharFunc read_char_func);


///
/// Receives a package and parses it
/// Currents are expressed in the Ampere, potentials are expressed in Volts
///
/// parameters:
///   MSComm 	 - The MSComm data struct
///   retData   - The package received is parsed and stored in this struct
///
/// Returns
///   CODE_OK if successful, CODE_MEASUREMENT_DONE if measurement is completed
///
RetCode ReceivePackage(MSComm* MSComm, MscrPackage* retData);


///
/// Parses a line of response and passes the meta data values for further parsing
///
/// parameters:
///   responseLine  - The line of response to be parsed
///   retData       - The struct in which the parsed values are stored
///
void ParseResponse(char *responseLine, MscrPackage* retData);


///
/// Splits the input string in to tokens based on the delimiters set (delim) and stores the pointer to the successive token in *stringp
/// This has to be performed repeatedly until end of string or until no further tokens are found
///
/// parameters:
///   stringp  - The pointer to the next string token
///   delim    - The array containing a set of delimiters
///
/// Returns:
///   The current token (char* ) found
///
char* strtokenize(char** stringp, const char* delim);


///
/// Look up function to convert the status value to a string
/// Note: only works when at most 1 status flag is set
///
/// parameters:
///   status   The status value to convert to a string
///
/// return:
///   The string that matches the `status` value. `"Undefined status"` if no exact match was found
///
const char* StatusToString(Status status);


///
/// Parses a parameter and calls the function to parse meta data values if any
///
/// parameters:
///   param    - The parameter value to be parsed
///   retData  - The struct in which the parsed values are stored
///
void ParseParam(char* param, MscrSubPackage* retData);


///
/// Retrieves the parameter value by parsing the input value and appending the SI unit prefix to it
///
/// Returns:
///   The actual parameter value in float (with its SI unit prefix)
///
float GetParameterValue(char* paramValue);


///
/// Parses the meta data values and calls the corresponding functions based on the meta data type (status, current range, noise)
///
/// parameters:
///   metaDataParams   - The meta data parameter values to be parsed
///   retData          - The struct in which the parsed values are stored
///
void ParseMetaDataValues(char *metaDataParams, MscrSubPackage* retData);


///
/// Parses the bytes corresponding to the status of the package(OK, Overload, Underload, Overload_warning)
///
/// parameters:
///   metaDataStatus - The meta data status value to be parsed
///
/// Returns:
///   A string corresponding to the parsed status
///
int GetStatusFromPackage(char* metaDataStatus);


///
/// Extracts the current range from the MethodSCRIPT package metadata
///
/// parameters:
///    metaDataCR - String containing (only) the metatdata current range field
///
/// return:
///    Bitmask value from the package
///
int GetCurrentRangeFromPackage(char* metaDataCR);


///
/// Returns the double value corresponding to the input unit prefix char
///
/// parameters:
///   charPrefix character representing the SI prefix
///
/// return:
///   Value corresponding to the SI prefix
///
const double GetUnitPrefixValue(char charPrefix);


///
/// Look up function to translate the current range value to a string.
///
/// parameters:
///   current_range The current range value from the MethodSCRIPT package
///
/// return:
///   Pointer to constant string containing the current range text
///
const char* current_range_to_string(int current_range);


///
/// Look up function to convert a MethodSCRIPT `variable type` value to a string
///
/// parameters:
///   variable_type The type of MethodSCRIP variable to find the sting for
///
/// return:
///   Variable type as (human readable) string
///
const char *VartypeToString(int variable_type);



//////////////////////////////////////////////////////////////////////////////
// Internal Communication Functions
//
// These functions are used by the SDK internally to communicate with the EmStat Pico.
// You probably don't need to use these directly, but they're here so you can if you want.
//////////////////////////////////////////////////////////////////////////////


///
/// Reads a character buffer using the supplied read_char_func
///
/// parameters:
///   MSComm  - The MSComm data struct
///   buf     - The buffer in which the response is stored
///
/// Returns
///   CODE_OK if successful, otherwise CODE_NULL.
///
RetCode ReadBuf(MSComm* MSComm, char* buf);


///
/// Reads a character using the supplied read_char_func
///
/// parameters:
///   MSComm - The MSComm data struct
///   c      - The character read is stored in this variable
///
/// Returns: CODE_OK if successful, otherwise CODE_TIMEOUT.
///
RetCode ReadChar(MSComm* MSComm, char* c);


///
/// Writes a character using the supplied write_char_func
///
/// parameters:
///   MSComm  - The MSComm data struct
///   c       - The character to be written
///
void WriteChar(MSComm* MSComm, char c);


///
/// Writes a 0 terminated string using the supplied write_char_func
///
/// parameters:
///   MSComm   - The MSComm data struct
///   buf      - The data to be written
///
void WriteStr(MSComm* MSComm, const char* buf);



#endif //MSComm_H
