/* ----------------------------------------------------------------------------
 *         PalmSens MethodSCRIPT SDK Example
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
/**
 *  Implementation of MethodSCRIPT. See header file and the "Getting started-MethodSCRIPT-Example-C.pdf" for more information.
 */
#include "MethodScriptExample.h"

HANDLE hCom;
DWORD dwBytesWritten = 0;
DWORD dwBytesRead;
MSComm msComm;
MeasureData data;
int nDataPoints;

const char* PORT_NAME = "\\\\.\\COM43";									   // The name of the port - to be changed, by looking up the device manager
const DWORD BAUD_RATE = 230400;											   // The baud rate for EmStat Pico

int main(int argc, char *argv[])
{
	RetCode code = MSCommInit(&msComm, &WriteToDevice, &ReadFromDevice);
	int continueParsing;
	if (code == CODE_OK)
	{
		int isOpen = OpenSerialPort();
		if(isOpen)
		{
			char buff[PATH_MAX];
			char *currentDirectory = getcwd(buff, PATH_MAX);		      // Fetches the current directory
			const char* filePath = "\\ScriptFiles\\LSV_on_10kOhm.txt"; 	  // "SWV_on_10kOhm.txt";
			int combinedFilePathSize = PATH_MAX + 1 + strlen(filePath);	  // Determines the max size of the combined file path
			char combinedFilePath[combinedFilePathSize];				  // An array to hold the combined file path
			if(currentDirectory != NULL)
			{
				char *combinedPath = strcat(currentDirectory, filePath);  // Concatenates the current directory and file path to generate the combined file path
				strcpy(combinedFilePath, combinedPath);
				if(SendScript(combinedFilePath))
				{
					printf("\nMethodSCRIPT sent to EmStat Pico.\n");
					nDataPoints = 0;
					do
					{
						code = ReceivePackage(&msComm, &data);			  // Receives the response and stores the parsed values in the struct 'data'
						continueParsing = DisplayResults(code);			  // Displays the results based on the returned code
					}while(continueParsing == 1);
				}
			}
			else{
				printf("Error in retrieving the current directory.");
			}
			if (hCom != NULL)
				CloseHandle(hCom);
		}
	}
}

int OpenSerialPort()
{
	DCB dcb = { 0 };
	BOOL fSuccess;
	hCom = CreateFile(PORT_NAME, GENERIC_READ | GENERIC_WRITE, 0,  		// must be opened with exclusive-access
									  NULL, 						    // no security attributes
									  OPEN_EXISTING,					// must use OPEN_EXISTING
									  0,								// not overlapped I/O
									  NULL								// hTemplate must be NULL for comm devices
									  );

	if (hCom == INVALID_HANDLE_VALUE) {
		printf("CreateFile failed with error %lu.\n", GetLastError());
		return 0;
	}

	// Set up the port connection parameters using device control block (DCB)
	fSuccess = GetCommState(hCom, &dcb);

	if (!fSuccess) {
		printf("GetCommState failed with error %lu.\n", GetLastError());
		return 0;
	}

	// Fill in DCB: 230400 bps, 8 data bits, no parity, and 1 stop bit.

	dcb.BaudRate = BAUD_RATE; 		// set the baud rate 230400 bps
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
	timeouts.ReadIntervalTimeout = 1000; 			// in milliseconds
	timeouts.ReadTotalTimeoutConstant = 1000; 		// in milliseconds
	timeouts.WriteTotalTimeoutConstant = 50; 		// in milliseconds
	timeouts.WriteTotalTimeoutMultiplier = 10; 		// in milliseconds

	if (!SetCommTimeouts(hCom, &timeouts)) {
		printf("SetCommState failed with error %lu.\n", GetLastError());
	}
	fflush(stdout);

	fSuccess = VerifyEmStatPico();					// Verifies if the connected device is EmStat Pico.
	if(fSuccess)
	{
		printf("Serial port successfully connected to EmStat Pico.\n");
		return 1;
	}
	printf("Connected device is not EmStat Pico.\n");
	return 0;
}

///
///Sends the command to get version string and verify if port is connected to Emstat pico.
///
BOOL VerifyEmStatPico()
{
	char versionString[30];
	RetCode code;
	BOOL isConnected = 0;
	WriteStr(&msComm, CMD_VERSION_STRING);
	do{
		code = ReadBuf(&msComm, versionString);
		if(code == CODE_VERSION_RESPONSE && strstr(versionString, "espico") != NULL) // Verifies if the device is EmStat Pico by looking for "espico" in the version response
			isConnected = 1;
		else if(strstr(versionString, "*\n") != NULL)								 // Reads until end of response and break
			break;
	}while(1);
	return isConnected;
}

int WriteToDevice(char c)
{
	char writeChar[2] = {c,'\0'};
	if (WriteFile(hCom, writeChar, 1, &dwBytesWritten, NULL))
	{
		return 1;
	}
	return 0;
}

int ReadFromDevice()
{
	char tempChar; 							// Temporary character used for reading
	DWORD noBytesRead;
	ReadFile(hCom, 							// Handle of the Serial port
			&tempChar, 						// Temporary character
			sizeof(tempChar),				// Size of TempChar
			&noBytesRead, 					// Number of bytes read
			NULL);
	return (int)tempChar;
}

int SendScript(char* fileName)
{
	FILE *fp;
	char str[100];

	fp = fopen(fileName, "r");
	if (fp == NULL) {
		printf("Could not open file %s", fileName);
		return 1;
	}
	while (fgets(str, 100, fp) != NULL)		// Reads a single line from the script file and sends it to the device.
	{
		WriteStr(&msComm, str);
	}
	fclose(fp);
	return 1;
}

int DisplayResults(RetCode code)
{
	int continueParsing = 1;
	switch(code)
	{
	case CODE_RESPONSE_BEGIN:				// Measurement response begins
		//do nothing
		break;
	case CODE_MEASURING:
		printf("\nMeasuring... \n");
		break;
	case CODE_OK:							// Received valid package, print it.
		if(nDataPoints == 0)
		printf("\nReceiving measurement response:\n");
		printf("\n %d \t", ++nDataPoints);
		printf("E(V): %6.3f \t", data.potential);
		fflush(stdout);
		printf("i(A) : %11.3E \t", data.current);
		printf("Status: %-15s  ", data.status);
		printf("CR: %s ", data.cr);
		fflush(stdout);
		break;
	case CODE_MEASUREMENT_DONE:			    // Measurement loop complete
		printf("\nMeasurement completed. ");
		break;
	case CODE_RESPONSE_END:				    // Measurement response end
		printf("%d data point(s) received.", nDataPoints);
	    fflush(stdout);
	    continueParsing = 0;
	    break;
	default:                    			// Failed to parse or identify package.
		printf("\nFailed to parse package: %d\n", code);
		continueParsing = 0;
	}
	return continueParsing;
}
