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
/**
 *  Implementation of the EmStat pico communication protocol. See header file for more information.
 */
#include "ESPicoCodeExample.h"

HANDLE hCom = 0;
DWORD dwBytesWritten = 0;
DWORD dwBytesRead;

int main(int argc, char *argv[])
{
	int isOpen = OpenSerialPort();
	if(isOpen)
	{
		//char* fileName = "C:\\EmStatPicoMaster\\emstatpico\\EmStatPicoExample-C\\EmStatPicoCodeExample\\EsPicoScriptFiles\\meas_swv_test.txt";
		char* fileName = "C:\\EmStatPicoMaster\\emstatpico\\EmStatPicoExample-C\\EmStatPicoCodeExample\\EsPicoScriptFiles\\LSV_test_script.txt";
		if(ReadScriptFile(fileName))
		{
			printf("\nScript file sent to EmStat Pico.\n");
			ReadMeasurementResponse();
		}
		if (hCom != NULL)
			CloseHandle(hCom);
	}
}

int OpenSerialPort()
{
	DCB dcb = { 0 };
	BOOL fSuccess;
	hCom = CreateFile("\\\\.\\COM37", GENERIC_READ | GENERIC_WRITE, 0,  // must be opened with exclusive-access
									  NULL, 						    // no security attributes
									  OPEN_EXISTING,					// must use OPEN_EXISTING
									  0,								// not overlapped I/O
									  NULL								// hTemplate must be NULL for comm devices
									  );

	if (hCom == INVALID_HANDLE_VALUE) {
		printf("CreateFile failed with error %lu.\n", GetLastError());
		return 0;
	}

	// Set up the port connection parameters with the help of device control block (DCB)

	fSuccess = GetCommState(hCom, &dcb);

	if (!fSuccess) {
		printf("GetCommState failed with error %lu.\n", GetLastError());
		return 0;
	}

	// Fill in DCB: 230400 bps, 8 data bits, no parity, and 1 stop bit.
	DWORD baud = 230400;
	dcb.BaudRate = baud; 			// set the baud rate 230400 bps
	dcb.ByteSize = 8; 				// data size
	dcb.Parity = NOPARITY; 			// no parity bit
	dcb.StopBits = ONESTOPBIT; 		// one stop bit


	fSuccess = SetCommState(hCom, &dcb);

	if (!fSuccess) {
		printf("SetCommState failed with error %lu.\n", GetLastError());
		return 0;
	}

	//Set the comm timeouts
	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 50; 			// in milliseconds
	timeouts.ReadTotalTimeoutConstant = 50; 	// in milliseconds
	timeouts.ReadTotalTimeoutMultiplier = 10; 	// in milliseconds
	timeouts.WriteTotalTimeoutConstant = 50; 	// in milliseconds
	timeouts.WriteTotalTimeoutMultiplier = 10; 	// in milliseconds

	if (!SetCommTimeouts(hCom, &timeouts)) {
		printf("SetCommState failed with error %lu.\n", GetLastError());
	}
	fflush(stdout);

	//Send the command to get version string and verify if port is connected to Emstat pico.
	char versionString[30];
	const char* cmdVersionString = "t\n";
	if(WriteToDevice(cmdVersionString))
	{
		RetCode ret = ReadBufferLine(versionString);
		if(ret != CODE_OK)
		{
			printf("Couldn't connect to EmStat pico.\n");
			return 0;
		}
		if(strstr(versionString, "esp") != NULL)
		{
			printf("Serial port successfully connected to EmStat pico.\n");
			return 1;
		}
	}
	return 0;
}

int WriteToDevice(const char* data) {
	DWORD len = strlen(data);
	if(WriteFile(hCom, data, len, &dwBytesWritten, NULL)) //Writes the data to the device and returns the number of bytes written
	{
		return 1;
	}
	return 0;
}

int ReadScriptFile(char* fileName)
{
	FILE *fp;
	char str[100];

	fp = fopen(fileName, "r");
	if (fp == NULL) {
		printf("Could not open file %s", fileName);
		return 1;
	}
	while (fgets(str, 100, fp) != NULL)		//Reads a single line from the script file and writes it on the device.
	{
		WriteToDevice(str);
	}
	fclose(fp);
	return 1;
}

RetCode ReadBufferLine(char* bufferLine)
{
	char TempChar; 							//Temporary character used for reading
	DWORD NoBytesRead;
	int i = 0;

	do {
		ReadFile(hCom, 						//Handle of the Serial port
				&TempChar, 					//Temporary character
				sizeof(TempChar),			//Size of TempChar
				&NoBytesRead, 				//Number of bytes read
				NULL);

		if(NoBytesRead > 0)
		{
			bufferLine[i++] = TempChar;		// Store Tempchar into buffer
			if(TempChar == '\n')
			{
				bufferLine[i] = '\0';
				if(strcmp(bufferLine, "*\n") == 0)
					return CODE_MEASUREMENT_DONE;
				else
					return CODE_OK;
			}
		}
	} while (i < 99);
	bufferLine[i] = '\0';
	return CODE_NULL;
}

void ReadMeasurementResponse()
{
	char bufferLine[100];
	int nDataPoints = 0;
	RetCode retCode = CODE_OK;
	printf("\nReceiving measurement response:");
	while(1)
	{
		memset(&(bufferLine[0]), 0, 100);								//Clears the buffer line
		retCode = ReadBufferLine(bufferLine);							//Reads a line from the device response
		if (bufferLine[0] == '\0' || retCode == CODE_MEASUREMENT_DONE)	//Exit if no line is read
			break;
		if(bufferLine[0] == 'P')										//Proceed to parse if the line begins with 'P'
		{
			nDataPoints++;												//Counts the number of data points received
			printf("\n");
			ParseResponse(bufferLine);									//Parses the line of response
		}
	}
	if(retCode == CODE_MEASUREMENT_DONE)
		printf("\nMeasurement completed, %d data points received.", nDataPoints);
}

void ParseResponse(char *responsePackageLine)
{
	char *P = strchr(responsePackageLine, 'P');							//Identifies the beginning of the response package
	char *packageLine = P+1;
	const char delimiters[] = " ;\n";
	char* running = packageLine;										// Initial index of the line to be tokenized
	char* param = strtokenize(&running, delimiters);					//Pulls out the parameters separated by the delimiters
	do
	{
		ParseParam(param);												//Parses the parameters further to get the meta data values if any

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

void ParseParam(char* param)
{
	char paramIdentifier[3];
	char paramValue[10];
	float parameterValue = 0;

	strncpy(paramIdentifier, param, 2);									//Splits the parameter identifier string
	paramIdentifier[2] = '\0';
	strncpy(paramValue, param+ 2, 8);									//Splits the parameter value string from
	paramValue[8]= '\0';
	parameterValue = (float)GetParameterValue(paramValue);				//Retrieves the actual parameter value
	if(strcmp(paramIdentifier, "da") == 0)
	{
		printf("E(V): %.3f \t", parameterValue);
		fflush(stdout);
	}
	else if (strcmp(paramIdentifier, "ba") == 0)
	{
		printf("i(A) : %1.3E \t", parameterValue);
		fflush(stdout);
	}
	ParseMetaDataVals(param + 10);										//Rest of the parameter is further parsed to get meta data values
}

float GetParameterValue(char* paramValue) {
	char charUnitPrefix = paramValue[7];						 		// Identify the SI unit prefix from the package at position 8
	char strValue[8];
	strncpy(strValue, paramValue, 7);
	strValue[7] = '\0';
	char *ptr;
	int value =	strtol(strValue, &ptr , 16);
	float parameterValue = value - OFFSET_VAL; 							// Values offset to receive only positive values
	return (parameterValue * GetUnitPrefixVal(charUnitPrefix));			//  Return the value of the parameter after appending the SI unit prefix
}

const double GetUnitPrefixVal(char charPrefix)
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

void ParseMetaDataVals(char *metaDataParams)
{
	const char delimiters[] = ",\n";
	char* running = metaDataParams;
	char* metaData = strtokenize(&running, delimiters);					//Splits the input string with meta data values based on set delimiters
	do
	{
		switch (metaData[0])
		{
			case '1':
				GetReadingStatusFromPackage(metaData);					//Retrieves the reading status of the parameter
				break;
			case '2':
				GetCurrentRangeFromPackage(metaData);					//Retrieves the current range of the parameter
				break;
			case '4':
																		//Retrieves the corresponding noise
				break;
		}
	}while ((metaData = strtokenize(&running, delimiters)) != NULL);
}

void GetReadingStatusFromPackage(char* metaDataStatus)
{
	char* status;
	char *ptr;
	long statusBits = strtol(&metaDataStatus[1], &ptr , 16);			//Fetches the status bit from the package
	if ((statusBits & 0x0) == 0x0)
		status = "OK";
	if ((statusBits & 0x2) == 0x2)
		status = "Overload";
	if ((statusBits & 0x4) == 0x4)
		status = "Underload";
	if ((statusBits & 0x8) == 0x8)
		status = "Overload warning";
	printf("Status: %s \t", status);
	fflush(stdout);
}


void GetCurrentRangeFromPackage(char* metaDataCR)
{
	char* currentRangeStr;
	char  crBytePackage[3];
	char* ptr;
	strncpy(crBytePackage, metaDataCR+1, 2);							//Fetches the current range bits from the package
	int crByte = strtol(crBytePackage, &ptr, 16);

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
	printf("CR: %s \t", currentRangeStr);
	fflush(stdout);
}

