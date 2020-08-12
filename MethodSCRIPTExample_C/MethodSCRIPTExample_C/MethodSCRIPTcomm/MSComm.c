/* ----------------------------------------------------------------------------
 *         PalmSens MethodSCRIPT SDK
 * ----------------------------------------------------------------------------
 Name        : MSComm.c
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
 */

#include "MSComm.h"


/// Offset value for MethodSCRIPT parameters (see the MethodSCRIPT documentation paragraph 'Measurement data package variables')
#define MSCR_PARAM_OFFSET_VALUE 0x8000000

/// The size of the serial read buffer in bytes. This is also the maximum length of a package
#define READ_BUFFER_LENGTH 1000


///
/// Reset a MscrSubPackage to a predefined state. Sets all metadata fields to -1 to indicate it is not written yet
///
void reset_mscr_subpackage(MscrSubPackage *subpackage)
{
	subpackage->value = 0;
	subpackage->variable_type = MSCR_STR_TO_VT("aa");

	// Clear metadata
	subpackage->metadata.status        = -1;
	subpackage->metadata.current_range = -1;

}


///
/// Rests a MscrPackage variable to a defined state
///
void reset_mscr_package(MscrPackage *package)
{
	package->nr_of_subpackages = 0;

	for (int i = 0; i < MSCR_SUBPACKAGES_PER_LINE; i++)
	{
		reset_mscr_subpackage(&package->subpackages[i]);
	}
}


///
///
///
RetCode MSCommInit(MSComm* msComm,	WriteCharFunc writeCharFunc, ReadCharFunc readCharFunc)
{
	msComm->writeCharFunc = writeCharFunc;			//Initializes the msComm with the function pointer to its write function
	msComm->readCharFunc = readCharFunc;			//Initializes the msComm with the function pointer to its read function

	if(writeCharFunc == NULL || readCharFunc == NULL)
	{
		return CODE_NULL;
	}
	return CODE_OK;
}


///
///
///
void WriteStr(MSComm* msComm, const char* buf)
{
	while(*buf != 0)
	{
		WriteChar(msComm, *buf);				//Writes the input array of characters to the device
		buf++;
	}
}


///
///
///
void WriteChar(MSComm* msComm, char c)
{
	msComm->writeCharFunc(c);
}


///
///
///
RetCode ReadBuf(MSComm* msComm, char* buf)
{
	int i = 0;
	do {
		int tempChar; 							//Temporary character used for reading
		tempChar = msComm->readCharFunc(); //Reads a character from the device
		if(tempChar > 0)
		{
			buf[i++] = tempChar;			//Stores tempchar into buffer

			if(tempChar == '\n')
			{
				buf[i] = '\0';
				if(buf[0] == REPLY_VERSION_RESPONSE)
					return CODE_VERSION_RESPONSE;
				else if(buf[0] == REPLY_MEASURING)
					return CODE_MEASURING;
				else if(strcmp(buf, "e\n") == 0)	//Wdg 20-11-2019 added
					return CODE_RESPONSE_BEGIN;		//..
				else if(strcmp(buf, "*\n") == 0)
					return CODE_MEASUREMENT_DONE;
				else if(strcmp(buf, "\n") == 0)
					return CODE_RESPONSE_END;
				else if(buf[0] == REPLY_MEASURE_DP)
					return CODE_OK;
				else
					return CODE_NOT_IMPLEMENTED;
			}
		}
	} while (i < READ_BUFFER_LENGTH-1);

	buf[i] = '\0';
	return CODE_NULL;
}


///
///
///
RetCode ReceivePackage(MSComm* msComm, MscrPackage* retData)
{
	char bufferLine[READ_BUFFER_LENGTH];
	RetCode ret = ReadBuf(msComm, bufferLine); // Reads a line of response from the device
	if (ret != CODE_OK)
		return ret;
	ParseResponse(bufferLine, retData);
	return ret;
}


///
///
///
void ParseResponse(char *responsePackageLine, MscrPackage* retData)
{
	reset_mscr_package(retData);

	char *P = strchr(responsePackageLine, 'P');					//Identifies the beginning of the response package
	char *packageLine = P+1;
	const char delimiters[] = ";\n";									//a space (" ") is used as prefix token thus part of parameter
	char* running = packageLine;										//Initial index of the line to be tokenized
	char* param;// = strtokenize(&running, delimiters);					//Pulls out the parameters separated by the delimiters
	int i = 0;
	while ((param = strtokenize(&running, delimiters)) != NULL)
	{
		if (strlen(param) == 0)
			continue;
		//Parse the parameters further to get the meta data values if any
		ParseParam(param, &retData->subpackages[i++]);
		retData->nr_of_subpackages++;
	}
}


///
///
///
char* strtokenize(char** stringp, const char* delim)
{
  char* start = *stringp;
  char* p;

  p = (start != NULL) ? strpbrk(start, delim) : NULL;					//Breaks the string when a delimiter is found and returns a pointer with starting index
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  		//Returns NULL if no delimiter is found
  if (p == NULL)
  {
    *stringp = NULL;													//The pointer to the successive token is set to NULL if no further tokens or end of string
  }
  else
  {
    *p = '\0';
    *stringp = p + 1;													//Saves the pointer to the beginning of successive token to be further tokenized
  }
  return start;															//Returns the current token found
}


///
///
///
void ParseParam(char* param, MscrSubPackage* retData)
{
	char paramIdentifier[3];
	char paramValue[10];

	strncpy(paramIdentifier, param, 2);									//Splits the parameter identifier string
	paramIdentifier[2] = '\0';
	strncpy(paramValue, param+ 2, 8);									//Splits the parameter value string
	paramValue[9]= '\0';
	retData->value = (float)GetParameterValue(paramValue);				//Retrieves the actual parameter value
	retData->variable_type = MSCR_STR_TO_VT(paramIdentifier);

	ParseMetaDataValues(param + 10, retData);							//Rest of the parameter is further parsed to get meta data values
}


///
///
///
float GetParameterValue(char* paramValue)
{
	char charUnitPrefix = paramValue[7]; 								//Identifies the SI unit prefix from the package at position 8
	char strValue[8];
	strncpy(strValue, paramValue, 7);
	strValue[7] = '\0';
	char *ptr;
	int value =	strtol(strValue, &ptr , 16);
	float parameterValue = value - MSCR_PARAM_OFFSET_VALUE; 						//Values offset to receive only positive values
	return (parameterValue * GetUnitPrefixValue(charUnitPrefix));		//Returns the value of the parameter after appending the SI unit prefix
}


///
///
///
const double GetUnitPrefixValue(char charPrefix)
{
	switch(charPrefix)
	{
		case 'a':
			return  1e-18;
		case 'f':
			return 1e-15;
		case 'p':
			return 1e-12;
		case 'n':
			return 1e-9;
		case 'u':
			return 1e-6;
		case 'm':
			return 1e-3;
		case ' ':
			return 1;
		case 'k':
			return 1e3;
		case 'M':
			return 1e6;
		case 'G':
			return 1e9;
		case 'T':
			return 1e12;
		case 'P':
			return 1e15;
		case 'E':
			return 1e18;
	}
	return 0;
}


///
///
///
void ParseMetaDataValues(char *metaDataParams, MscrSubPackage* retData)
{
	const char delimiters[] = ",\n";
	char* running = metaDataParams;
	char* metaData = strtokenize(&running, delimiters);					  	//Splits the input string with meta data values based on set delimiters
	do
	{
		switch (metaData[0])
		{
			case '1':
				retData->metadata.status = GetStatusFromPackage(metaData); 	//Retrieves the reading status of the parameter
				break;
			case '2':
				retData->metadata.current_range = GetCurrentRangeFromPackage(metaData);		    //Retrieves the current range of the parameter
				break;
		}
	} while ((metaData = strtokenize(&running, delimiters)) != NULL);
}


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
const char* StatusToString(Status status)
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


///
/// Read the status bitmask from a package
///
/// parameters:
///    metaDataStatus string containing (only) the metadata status field
///
/// return:
///    Bitmask value from the package
///
int GetStatusFromPackage(char* metaDataStatus)
{
	int statusBits = strtol(&metaDataStatus[1], NULL, 16);
	return statusBits;
}


///
/// Extracts the current range from the MethodSCRIPT package metadata
///
/// parameters:
///    metaDataCR String containing (only) the metatdata current range field
///
/// return:
///    Bitmask value from the package
///
int GetCurrentRangeFromPackage(char* metaDataCR)
{
	char  crBytePackage[3];
	strncpy(crBytePackage, metaDataCR + 1, 2);
	return strtol(crBytePackage, NULL, 16);
}


///
/// Look up function to translate the current range value to a string.
///
/// parameters:
///   current_range The current range value from the MethodSCRIPT package
///
/// return:
///   Pointer to constant string containing the current range text
///
const char* current_range_to_string(int current_range)
{
	switch (current_range)
	{
		case 0:
			return "100nA";
		case 1:
			return "2uA";
		case 2:
			return "4uA";
		case 3:
			return "8uA";
		case 4:
			return "16uA";
		case 5:
			return "32uA";
		case 6:
			return "63uA";
		case 7:
			return "125uA";
		case 8:
			return "250uA";
		case 9:
			return "500uA";
		case 10:
			return "1mA";
		case 11:
			return "15mA";
		case 128:
			return "100nA (High speed)";
		case 129:
			return "1uA (High speed)";
		case 130:
			return "6uA (High speed)";
		case 131:
			return "13uA (High speed)";
		case 132:
			return "25uA (High speed)";
		case 133:
			return "50uA (High speed)";
		case 134:
			return "100uA (High speed)";
		case 135:
			return "200uA (High speed)";
		case 136:
			return "1mA (High speed)";
		case 137:
			return "5mA (High speed)";
		default:
			return "Invalid value";
	}
}


///
/// Look up function to convert a MethodSCRIPT `variable type` value to a string
///
const char *VartypeToString(int variable_type)
{
	switch(variable_type)
	{
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
		case MSCR_VT_NONE:
			return "None";
		default:
			return "Undefined variable type";
	}
}
