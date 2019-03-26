/* ----------------------------------------------------------------------------
 *         PalmSens MethodSCRIPT SDK
 * ----------------------------------------------------------------------------
 Name        : MSComm.c
 Copyright   :
 * ----------------------------------------------------------------------------
 * Copyright (c) 2019, PalmSens BV
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

const int OFFSET_VALUE = 0x8000000;
const int READ_BUFFER_LENGTH = 100;

RetCode MSCommInit(MSComm* msComm,	WriteCharFunc writeCharFunc, ReadCharFunc readCharFunc)
{
	msComm->writeCharFunc = writeCharFunc;			//Initialises the msComm with the function pointer to its write function
	msComm->readCharFunc = readCharFunc;			//Initialises the msComm with the function pointer to its read function

	if(writeCharFunc == NULL || readCharFunc == NULL)
	{
		return CODE_NULL;
	}
	return CODE_OK;
}

void WriteStr(MSComm* msComm, const char* buf)
{
	while(*buf != 0)
	{
		WriteChar(msComm, *buf);				//Writes the input array of characters to the device
		buf++;
	}
}

void WriteChar(MSComm* msComm, char c)
{
	msComm->writeCharFunc(c);
}

RetCode ReadBuf(MSComm* msComm, char* buf)
{
	int tempChar; 							//Temporary character used for reading
	int i = 0;
	do {
		tempChar = msComm->readCharFunc(); //Reads a character from the device
		if(tempChar > 0)
		{
			buf[i++] = tempChar;			//Stores tempchar into buffer
			if(buf[0] == (int)'e')
				return CODE_RESPONSE_BEGIN;
			if(tempChar == '\n')
			{
				buf[i] = '\0';
				if(buf[0] == REPLY_VERSION_RESPONSE)
					return CODE_VERSION_RESPONSE;
				if(buf[0] == REPLY_MEASURING)
					return CODE_MEASURING;
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
	} while (i < READ_BUFFER_LENGTH);
	buf[i] = '\0';
	return CODE_NULL;
}

RetCode ReceivePackage(MSComm* msComm, MeasureData* retData)
{
	char bufferLine[100];
	RetCode ret = ReadBuf(msComm, bufferLine);							// Reads a line of response from the device
	if (ret != CODE_OK)
		return ret;
    ParseResponse(bufferLine, retData);									// Parses the response
	return ret;
}

void ParseResponse(char *responsePackageLine, MeasureData* retData)
{
	char *P = strchr(responsePackageLine, 'P');							//Identifies the beginning of the response package
	char *packageLine = P+1;
	const char delimiters[] = " ;\n";
	char* running = packageLine;										//Initial index of the line to be tokenized
	char* param = strtokenize(&running, delimiters);					//Pulls out the parameters separated by the delimiters
	do
	{
		ParseParam(param, retData);									    //Parses the parameters further to get the meta data values if any

	}while ((param = strtokenize(&running, delimiters)) != NULL);		//Continues parsing the response line until end of line
}

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

void ParseParam(char* param, MeasureData* retData)
{
	char paramIdentifier[3];
	char paramValue[10];
	float parameterValue = 0;

	strncpy(paramIdentifier, param, 2);									//Splits the parameter identifier string
	paramIdentifier[2] = '\0';
	strncpy(paramValue, param+ 2, 8);									//Splits the parameter value string
	paramValue[9]= '\0';
	parameterValue = (float)GetParameterValue(paramValue);				//Retrieves the actual parameter value
	if(strcmp(paramIdentifier, "da") == 0)
	{
		retData->potential = parameterValue;
	}
	else if (strcmp(paramIdentifier, "ba") == 0)
	{
		retData->current = parameterValue;
	}
	ParseMetaDataValues(param + 10, retData);							//Rest of the parameter is further parsed to get meta data values
}

float GetParameterValue(char* paramValue)
{
	char charUnitPrefix = paramValue[7]; 								//Identifies the SI unit prefix from the package at position 8
	char strValue[8];
	strncpy(strValue, paramValue, 7);
	strValue[7] = '\0';
	char *ptr;
	int value =	strtol(strValue, &ptr , 16);
	float parameterValue = value - OFFSET_VALUE; 						//Values offset to receive only positive values
	return (parameterValue * GetUnitPrefixValue(charUnitPrefix));		//Returns the value of the parameter after appending the SI unit prefix
}

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
		case 'K':
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

void ParseMetaDataValues(char *metaDataParams, MeasureData* retData)
{
	const char delimiters[] = ",\n";
	char* running = metaDataParams;
	char* metaData = strtokenize(&running, delimiters);					  	//Splits the input string with meta data values based on set delimiters
	do
	{
		switch (metaData[0])
		{
			case '1':
				retData->status = GetReadingStatusFromPackage(metaData); 	//Retrieves the reading status of the parameter
				break;
			case '2':
				retData->cr = GetCurrentRangeFromPackage(metaData);		    //Retrieves the current range of the parameter
				break;
			case '4':
																			//Retrieves the corresponding noise
				break;
		}
	}while ((metaData = strtokenize(&running, delimiters)) != NULL);
}

char* StatusToString(Status status)
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
			return "Invalid status";
    }
}

const char* GetReadingStatusFromPackage(char* metaDataStatus)
{
	char* status;
	long statusBits = strtol(&metaDataStatus[1], NULL, 16);				//Fetches the status bit from the package
	if ((statusBits) == STATUS_OK)
		status = StatusToString(STATUS_OK);
	if ((statusBits & 0x2) == STATUS_OVERLOAD)
		status = StatusToString(STATUS_OVERLOAD);
	if ((statusBits & 0x4) == STATUS_UNDERLOAD)
		status = StatusToString(STATUS_UNDERLOAD);
	if ((statusBits & 0x8) == STATUS_OVERLOAD_WARNING)
		status = StatusToString(STATUS_OVERLOAD_WARNING);
	return status;
}


const char* GetCurrentRangeFromPackage(char* metaDataCR)
{
	char* currentRangeStr;
	char  crBytePackage[3];
	strncpy(crBytePackage, metaDataCR+1, 2);							//Fetches the current range bits from the package
	int crByte = strtol(crBytePackage, NULL, 16);

	switch (crByte)
	{
		case 0:
			currentRangeStr = "100nA";
			break;
		case 1:
			currentRangeStr = "2uA";
			break;
		case 2:
			currentRangeStr = "4uA";
			break;
		case 3:
			currentRangeStr = "8uA";
			break;
		case 4:
			currentRangeStr = "16uA";
			break;
		case 5:
			currentRangeStr = "32uA";
			break;
		case 6:
			currentRangeStr = "63uA";
			break;
		case 7:
			currentRangeStr = "125uA";
			break;
		case 8:
			currentRangeStr = "250uA";
			break;
		case 9:
			currentRangeStr = "500uA";
			break;
		case 10:
			currentRangeStr = "1mA";
			break;
		case 11:
			currentRangeStr = "15mA";
			break;
		case 128:
			currentRangeStr = "100nA (High speed)";
			break;
		case 129:
			currentRangeStr = "1uA (High speed)";
			break;
		case 130:
			currentRangeStr = "6uA (High speed)";
			break;
		case 131:
			currentRangeStr = "13uA (High speed)";
			break;
		case 132:
			currentRangeStr = "25uA (High speed)";
			break;
		case 133:
			currentRangeStr = "50uA (High speed)";
			break;
		case 134:
			currentRangeStr = "100uA (High speed)";
			break;
		case 135:
			currentRangeStr = "200uA (High speed)";
			break;
		case 136:
			currentRangeStr = "1mA (High speed)";
			break;
		case 137:
			currentRangeStr = "5mA (High speed)";
			break;
	}
	return currentRangeStr;
}

