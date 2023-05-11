/**
 * PalmSens MethodSCRIPT SDK example for Arduino
 *
 * See documentation in MSComm.h.
 *
 * ----------------------------------------------------------------------------
 *
 *	\copyright (c) 2019-2021 PalmSens BV
 *	All rights reserved.
 *
 *	Redistribution and use in source and binary forms, with or without
 *	modification, are permitted provided that the following conditions are met:
 *
 *		- Redistributions of source code must retain the above copyright notice,
 *		  this list of conditions and the following disclaimer.
 *		- Neither the name of PalmSens BV nor the names of its contributors
 *		  may be used to endorse or promote products derived from this software
 *		  without specific prior written permission.
 *		- This license does not release you from any requirement to obtain separate 
 *		  licenses from 3rd party patent holders to use this software.
 *		- Use of the software either in source or binary form must be connected to, 
 *		  run on or loaded to an PalmSens BV component.
 *
 *	DISCLAIMER: THIS SOFTWARE IS PROVIDED BY PALMSENS "AS IS" AND ANY EXPRESS OR
 *	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 *	EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *	LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 *	OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *	EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ----------------------------------------------------------------------------
 */

#include "MSComm.h"


/// Offset value for MethodSCRIPT parameters (see the MethodSCRIPT documentation paragraph 'Measurement data package variables')
#define MSCR_PARAM_OFFSET_VALUE 0x8000000

/// The size of the serial read buffer in bytes. This is also the maximum length of a package
#define READ_BUFFER_LENGTH 1000

static DeviceType s_dt;
//
// See documentation in MSComm.h
//
void reset_mscr_subpackage(MscrSubPackage * subpackage)
{
	subpackage->value = 0;
	subpackage->variable_type = MSCR_STR_TO_VT("aa");

	// Clear metadata
	subpackage->metadata.status = -1;
	subpackage->metadata.range  = -1;
}

//
// See documentation in MSComm.h
//
void reset_mscr_package(MscrPackage * package)
{
	package->nr_of_subpackages = 0;

	for (int i = 0; i < MSCR_SUBPACKAGES_PER_LINE; i++) {
		reset_mscr_subpackage(&package->subpackages[i]);
	}
}

//
// See documentation in MSComm.h
//
RetCode MSCommInit(MSComm * msComm,	DeviceType dt, 
						WriteCharFunc writeCharFunc, ReadCharFunc readCharFunc)
{
	// Initialize msComm with the function pointer to its write and read function
	msComm->writeCharFunc = writeCharFunc;
	msComm->readCharFunc = readCharFunc;

	if ((writeCharFunc == NULL) || (readCharFunc == NULL)) {
		return CODE_NULL;
	}
	
	s_dt = dt;
	
	return CODE_OK;
}

//
// See documentation in MSComm.h
//
void WriteStr(MSComm * msComm, char const * buf)
{
	// Write the input array of characters to the device
	while (*buf != '\0') {
		WriteChar(msComm, *buf++);
	}
}

//
// See documentation in MSComm.h
//
void WriteChar(MSComm * msComm, char c)
{
	msComm->writeCharFunc(c);
}

//
// See documentation in MSComm.h
//
RetCode ReadBuf(MSComm * msComm, char * buf)
{
	int i = 0;
	do {
		// Read a character from the device
		int tempChar = msComm->readCharFunc();
		if (tempChar > 0) {
			// Store character into buffer
			buf[i++] = tempChar;

			if (tempChar == '\n') {
				buf[i] = '\0';
				if (buf[0] == REPLY_VERSION_RESPONSE) {
					return CODE_VERSION_RESPONSE;
				} else if ((buf[0] == REPLY_MEASURING) || (buf[0] == REPLY_NSCANS_START)) {
					return CODE_MEASURING;
				} else if (strcmp(buf, "e\n") == 0) {
					return CODE_RESPONSE_BEGIN;
				} else if ((strcmp(buf, "*\n") == 0 || strcmp(buf, "-\n") == 0)) {
					return CODE_MEASUREMENT_DONE;
				} else if (strcmp(buf, "\n") == 0) {
					return CODE_RESPONSE_END;
				} else if (buf[0] == REPLY_MEASURE_DP) {
					return CODE_OK;
				} else {
					printf("Unexpected response from ES Pico: \"%s\"\n", buf);
					return CODE_NOT_IMPLEMENTED;
				}
			}
		}
	} while (i < (READ_BUFFER_LENGTH - 1));

	buf[i] = '\0';
	return CODE_NULL;
}

//
// See documentation in MSComm.h
//
RetCode ReceivePackage(MSComm * msComm, MscrPackage * retData)
{
	// Read a line of response from the device
	char bufferLine[READ_BUFFER_LENGTH];
	RetCode ret = ReadBuf(msComm, bufferLine);
	if (ret != CODE_OK) {
		return ret;
	}
	ParseResponse(bufferLine, retData);
	return ret;
}

//
// See documentation in MSComm.h
//
void ParseResponse(char * responsePackageLine, MscrPackage * retData)
{
	reset_mscr_package(retData);

	// Identify the beginning of the response package
	char * P = strchr(responsePackageLine, 'P');
	char * packageLine = P + 1;
	// Initial index of the line to be tokenized
	char * running = packageLine;
	// Pull out the parameters separated by the delimiters
	char * param;
	int i = 0;

	while ((param = strtokenize(&running, ";\n")) != NULL) {
		if (strlen(param) == 0) {
			continue;
		}
		//Parse the parameters further to get the meta data values if any
		ParseParam(param, &retData->subpackages[i++]);
		retData->nr_of_subpackages++;
	}
}

//
// See documentation in MSComm.h
//
char * strtokenize(char * * stringp, char const * delim)
{
	char * start = *stringp;
	char * p;

	// Break the string when a delimiter is found and return a pointer with starting index
	// Return NULL if no delimiter is found
	p = (start != NULL) ? strpbrk(start, delim) : NULL;
	if (p == NULL) {
		// The pointer to the successive token is set to NULL if no further tokens or end of string
		*stringp = NULL;
	} else {
		*p = '\0';
		// Save the pointer to the beginning of successive token to be further tokenized
		*stringp = p + 1;
	}
	// Return the current token found
	return start;
}

//
// See documentation in MSComm.h
//
void ParseParam(char * param, MscrSubPackage * retData)
{
	char paramIdentifier[3];
	char paramValue[10];

	// Split the parameter identifier string
	strncpy(paramIdentifier, param, 2);
	paramIdentifier[2] = '\0';
	// Split the parameter value string
	strncpy(paramValue, param+ 2, 8);
	paramValue[9]= '\0';
	// Retrieve the actual parameter value
	retData->value = GetParameterValue(paramValue);
	retData->variable_type = MSCR_STR_TO_VT(paramIdentifier);

	// Rest of the parameter is further parsed to get meta data values
	ParseMetaDataValues(param + 10, retData);
}

//
// See documentation in MSComm.h
//
float GetParameterValue(char * paramValue)
{
	// Identify the SI unit prefix from the package at position 8
	char charUnitPrefix = paramValue[7];
	char strValue[8];
	strncpy(strValue, paramValue, 7);
	strValue[7] = '\0';
	char *ptr;
	int32_t value =	strtol(strValue, &ptr , 16);
	// Values offset to receive only positive values
	float parameterValue = value - MSCR_PARAM_OFFSET_VALUE;
	// Return the value of the parameter after appending the SI unit prefix
	return (parameterValue * GetUnitPrefixValue(charUnitPrefix));
}

//
// See documentation in MSComm.h
//
const double GetUnitPrefixValue(char charPrefix)
{
	switch (charPrefix)
	{
	case 'a': return 1e-18; // atto
	case 'f': return 1e-15; // femto
	case 'p': return 1e-12; // pico
	case 'n': return 1e-9;  // nano
	case 'u': return 1e-6;  // micro
	case 'm': return 1e-3;  // milli
	case ' ': return 1;
	case 'k': return 1e3;   // kilo
	case 'M': return 1e6;   // mega
	case 'G': return 1e9;   // giga
	case 'T': return 1e12;  // tera
	case 'P': return 1e15;  // peta
	case 'E': return 1e18;  // exa
	}
	return 0;
}

//
// See documentation in MSComm.h
//
void ParseMetaDataValues(char * metaDataParams, MscrSubPackage * retData)
{
	char const delimiters[] = ",\n";
	char * running = metaDataParams;
	// Split the input string with meta data values based on set delimiters
	char * metaData = strtokenize(&running, delimiters);
	do {
		switch (metaData[0]) {
		case '1':
			// Retrieve the reading status of the parameter
			retData->metadata.status = GetStatusFromPackage(metaData);
			break;
		case '2':
			// Retrieve the current or potential range of the parameter
			retData->metadata.range = GetRangeFromPackage(metaData);
			break;
		}
	} while ((metaData = strtokenize(&running, delimiters)) != NULL);
}

//
// See documentation in MSComm.h
//
char const * StatusToString(Status status)
{
	switch(status){
	case STATUS_OK:
		return "OK";
	case STATUS_OVERLOAD:
		return "Overload";
	case STATUS_UNDERLOAD:
		 return "Underload";
	case STATUS_OVERLOAD_WARNING:
		 return "Overload warning";
	default:
		return "Undefined status";
	}
}

//
// See documentation in MSComm.h
//
int GetStatusFromPackage(char * metaDataStatus)
{
	return strtol(&metaDataStatus[1], NULL, 16);
}


//
// See documentation in MSComm.h
//
int GetRangeFromPackage(char * metaDataRange)
{
	char rangeBytePackage[3];
	strncpy(rangeBytePackage, metaDataRange + 1, 2);
	return strtol(rangeBytePackage, NULL, 16);
}

//
// See documentation in MSComm.h
//
char const * range_to_string(int range, VarType vt)
{
	if(s_dt == DT_ESPICO)
	{
		switch (range)
		{
		case   0: return "100 nA";
		case   1: return   "2 uA";
		case   2: return   "4 uA";
		case   3: return   "8 uA";
		case   4: return  "16 uA";
		case   5: return  "32 uA";
		case   6: return  "63 uA";
		case   7: return "125 uA";
		case   8: return "250 uA";
		case   9: return "500 uA";
		case  10: return   "1 mA";
		case  11: return   "5 mA";
		case 128: return "100 nA (High speed)";
		case 129: return   "1 uA (High speed)";
		case 130: return   "6 uA (High speed)";
		case 131: return  "13 uA (High speed)";
		case 132: return  "25 uA (High speed)";
		case 133: return  "50 uA (High speed)";
		case 134: return "100 uA (High speed)";
		case 135: return "200 uA (High speed)";
		case 136: return   "1 mA (High speed)";
		case 137: return   "5 mA (High speed)";
		default:  return "Invalid value";
		}
	}
	else if(s_dt == DT_ES4)
	{
		if(vt == MSCR_VT_POTENTIAL)
		{
			switch (range)
			{
			case   2: return   "50 mV";
			case   3: return  "100 mV";
			case   4: return  "200 mV";
			case   5: return  "500 mV";
			case   6: return     "1 V";
			default:  return "Invalid value";
			}
		}
		else
		{
			switch (range)
			{
			case   0: return "100 pA";
			case   3: return   "1 nA";
			case   6: return  "10 nA";
			case   9: return "100 nA";
			case   12:return   "1 uA";
			case   15:return  "10 uA";
			case   18:return "100 uA";
			case   21:return   "1 mA";
			case   24:return  "10 mA";
			case   27:return "100 mA";
			default:  return "Invalid value";
			}
		}
	}
}

//
// See documentation in MSComm.h
//
const char * VartypeToString(int variable_type)
{
	switch (variable_type) {
	case MSCR_VT_UNKNOWN:
		return "MSCR_VT_UNKNOWN";
	case MSCR_VT_POTENTIAL:
		return "Potential";
	case MSCR_VT_POTENTIAL_CE:
		return "Potential_CE";
	case MSCR_VT_POTENTIAL_SE:
		return "Potential_SE";
	case MSCR_VT_POTENTIAL_RE:
		return "Potential_RE";
	case MSCR_VT_POTENTIAL_WE_VS_CE:
		return "Potential_WE_vs_CE";
	case MSCR_VT_POTENTIAL_AIN0:
		return "Potential_AIN0";
	case MSCR_VT_POTENTIAL_AIN1:
		return "Potential_AIN1";
	case MSCR_VT_POTENTIAL_AIN2:
		return "Potential_AIN2";
	case MSCR_VT_POTENTIAL_AIN3:
		return "Potential_AIN3";
	case MSCR_VT_POTENTIAL_AIN4:
		return "Potential_AIN4";
	case MSCR_VT_POTENTIAL_AIN5:
		return "Potential_AIN5";
	case MSCR_VT_POTENTIAL_AIN6:
		return "Potential_AIN6";
	case MSCR_VT_POTENTIAL_AIN7:
		return "Potential_AIN7";
	case MSCR_VT_CURRENT:
		return "Current";
	case MSCR_VT_PHASE:
		return "Phase";
	case MSCR_VT_IMP:
		return "Imp";
	case MSCR_VT_ZREAL:
		return "Zreal";
	case MSCR_VT_ZIMAG:
		return "Zimag";
	case MSCR_VT_CELL_SET_POTENTIAL:
		return "Cell_set_potential";
	case MSCR_VT_CELL_SET_CURRENT:
		return "Cell_set_current";
	case MSCR_VT_CELL_SET_FREQUENCY:
		return "Cell_set_frequency";
	case MSCR_VT_CELL_SET_AMPLITUDE:
		return "Cell_set_amplitude";
	case MSCR_VT_CHANNEL:
		return "Channel";
	case MSCR_VT_TIME:
		return "Time";
	case MSCR_VT_PIN_MSK:
		return "Pin_msk";
	case MSCR_VT_DEV_ADC_OFFSET:
		return "Dev_adc_offset";
	case MSCR_VT_DEV_HS_EX:
		return "Dev_hs_ex";
	case MSCR_VT_CURRENT_GENERIC1:
		return "Current_generic1";
	case MSCR_VT_CURRENT_GENERIC2:
		return "Current_generic2";
	case MSCR_VT_CURRENT_GENERIC3:
		return "Current_generic3";
	case MSCR_VT_CURRENT_GENERIC4:
		return "Current_generic4";
	case MSCR_VT_POTENTIAL_GENERIC1:
		return "Potential_generic1";
	case MSCR_VT_POTENTIAL_GENERIC2:
		return "Potential_generic2";
	case MSCR_VT_POTENTIAL_GENERIC3:
		return "Potential_generic3";
	case MSCR_VT_POTENTIAL_GENERIC4:
		return "Potential_generic4";
	case MSCR_VT_MISC_GENERIC1:
		return "Misc_generic1";
	case MSCR_VT_MISC_GENERIC2:
		return "Misc_generic2";
	case MSCR_VT_MISC_GENERIC3:
		return "Misc_generic3";
	case MSCR_VT_MISC_GENERIC4:
		return "Misc_generic4";
	default:
		return "Undefined variable type";
	}
}
