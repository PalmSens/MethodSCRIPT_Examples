/* ----------------------------------------------------------------------------
 *         PalmSens MethodSCRIPT SDK Example
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
/**
 *  Implementation of MethodSCRIPT. See header file and the "Getting started-MethodSCRIPT-Example-C.pdf" for more information.
 */
#include "MethodSCRIPTExample.h"

#define MS_MAX_LINECHARS	128

HANDLE hCom;				// Serial port handle
DWORD dwBytesWritten = 0;	// Number of bytes written variable for serial WriteFile function
MSComm msComm;				// MethodScript communication interface
FILE *pFCsv;				// CSV File pointer


const char* PORT_NAME = "\\\\.\\COM23";									   // The name of the port - to be changed, by looking up the device manager
const DWORD BAUD_RATE = 230400;											   // The baud rate for EmStat Pico


// Select demo
// 0 = LSV (with 10k Ohm)
// 1 = EIS
// 2 = SWV (with 10k Ohm)
#define DEMO_SELECT	0

#if DEMO_SELECT == 0
	const char* METHODSCRIPT_FILEPATHNAME = "\\ScriptFiles\\MSExample_LSV_10k.mscr";	// LSV demo
	const char* RESULT_FILEPATHNAME = 		".\\Results\\MSExample_LSV_10k.csv";
#elif DEMO_SELECT == 1
	const char* METHODSCRIPT_FILEPATHNAME = "\\ScriptFiles\\MSExample_EIS.mscr";		// EIS demo
	const char* RESULT_FILEPATHNAME = 		".\\Results\\MSExample_EIS.csv";
#elif DEMO_SELECT == 2
	const char* METHODSCRIPT_FILEPATHNAME = "\\ScriptFiles\\MSExample_SWV_10k.mscr";	// SWV demo
	const char* RESULT_FILEPATHNAME = 		".\\Results\\MSExample_SWV_10k.csv";
#endif


int main(int argc, char *argv[])
{
	MeasureData data;
	int nDataPoints;
	RetCode status_code = MSCommInit(&msComm, &WriteToDevice, &ReadFromDevice);

	if (status_code == CODE_OK)
	{
		int isOpen = OpenSerialPort();
		if(isOpen)
		{
			int fSuccess = VerifyEmStatPico();					// Verifies if the connected device is EmStat Pico.
			if(fSuccess)
			{
				printf("Serial port successfully connected to EmStat Pico.\n");
			} else {
				printf("Connected device is not EmStat Pico.\n");
				return -1;
			}

			char buff[PATH_MAX];
			char *currentDirectory = getcwd(buff, PATH_MAX);		      // Fetches the current directory
			if(currentDirectory != NULL)
			{
				const char* filePath = METHODSCRIPT_FILEPATHNAME; 	  			// MethodScript Filename incl. path
				int combinedFilePathSize = PATH_MAX + 1 + strlen(filePath);	  	// Determines the max size of the combined file path
				char combinedFilePath[combinedFilePathSize];				  	// An array to hold the combined file path
				char *combinedPath = strcat(currentDirectory, filePath);  // Concatenates the current directory and file path to generate the combined file path
				strcpy(combinedFilePath, combinedPath);
				if(SendScriptFile(combinedFilePath))
				{
					printf("\nMethodSCRIPT sent to EmStat Pico.\n");
					nDataPoints = 0;
					int continueParsing;
					do
					{
						status_code = ReceivePackage(&msComm, &data);			// Receives the response and stores the parsed values in the struct 'data'
						ResultsToCsv(status_code, data, nDataPoints);								// Write result data-point to a CSV file
						continueParsing = DisplayResults(status_code, data, &nDataPoints);			// Displays the results based on the returned code
					}while(continueParsing == 1);
				}
			}
			else{
				printf("Error in retrieving the current directory.");
			}
			if (hCom != NULL)
				CloseHandle(hCom);
		} else {
			printf("ERROR: Could not open serial port [%s].\r\n", strrchr(PORT_NAME,'\\') + 1);
		}
	} else {
		printf("ERROR: Failed to configure MSComm object.\r\n");
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
		return 0;
	}
	fflush(stdout);

	return 1;
}


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
	}while(1);											//Note: this example expects a connected Emstat Pico and hangs otherwise!
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
	//Check for timeout
	if(noBytesRead != sizeof(tempChar))
		return -1;							//Return -1 on timeout
	return (int)tempChar;
}

int SendScriptFile(char* fileName)
{
	FILE *fp;
	char str[MS_MAX_LINECHARS+1];	//Plus the string termination character (0)

	fp = fopen(fileName, "r");
	if (fp == NULL) {
		printf("Could not open file %s", fileName);
		return 0;
	}
	// Reads a single line from the script file and sends it to the device.
	while (fgets(str, MS_MAX_LINECHARS, fp) != NULL)
	{
		WriteStr(&msComm, str);
	}
	fclose(fp);
	return 1;
}

int DisplayResults(RetCode code, MeasureData result, int *nDataPoints)
{
	int continueParsing = 1;
	switch(code)
	{
	case CODE_RESPONSE_BEGIN:				// Measurement response begins
		printf("\nResponse begin\n");
		break;
	case CODE_MEASURING:
		printf("\nMeasuring... \n");
		break;
	case CODE_OK:							// Received valid package, print it.
		if(*nDataPoints == 0)
			printf("\nReceiving measurement response:\n");
		printf("\n %d \t", ++*nDataPoints);
		if (result.zreal != HUGE_VALF)		//Check if it is EIS data
		{
			printf("frequency(Hz): %8.1f \t", result.frequency);
			printf("Zreal(Ohm): %16.3f \t", result.zreal);
			printf("Zimag(Ohm): %16.3f \t", result.zimag);
			fflush(stdout);
		}
		else
		{
			printf("E(V): %6.3f \t", result.potential);
			printf("i(A) : %11.3E \t", result.current);
			fflush(stdout);
		}
		printf("Status: %-15s  ", result.status);
		printf("CR: %s ", result.cr);
		fflush(stdout);
		break;
	case CODE_MEASUREMENT_DONE:			    // Measurement loop complete
		printf("\nMeasurement completed. ");
		break;
	case CODE_RESPONSE_END:				    // Measurement response end
		printf("%d data point(s) received.", *nDataPoints);
	    fflush(stdout);
	    continueParsing = 0;
	    break;
	default:                    			// Failed to parse or identify package.
		printf("\nFailed to parse package: %d\n", code);
		continueParsing = 0;
	}
	return continueParsing;
}

void ResultsToCsv(const RetCode code, const MeasureData result, const int nDataPoints)
{
	switch(code)
	{
	case CODE_RESPONSE_BEGIN:					// Measurement response begins
		OpenCSVFile(RESULT_FILEPATHNAME, &pFCsv);
		break;
	case CODE_MEASURING:
		break;
	case CODE_OK:								// Received valid package, print it.
		if(nDataPoints == 0)
		{
			WriteHeaderToCSVFile(pFCsv);
		}
		WriteDataToCSVFile(pFCsv, result, nDataPoints);
		break;
	case CODE_MEASUREMENT_DONE:			    	// Measurement loop complete
		fclose(pFCsv);
		break;
	case CODE_RESPONSE_END:				    	// Measurement response end
	    break;
	default:                    				// Failed to parse or identify package.
		break;
	}
}

void OpenCSVFile(const char *pFilename, FILE **fp)
{
	*fp = fopen(pFilename, "w");		//Open file for writing (overwrite existing)
	if (*fp == NULL)
	{
		printf("Could not open CSV file %s (hint: make sure the directory exists)", pFilename);
	}
}

void WriteHeaderToCSVFile(FILE *fp)
{
	if (fp == NULL)
	{
		printf("Unable to write header to CSV file\r\n");
		fflush(stdout);
		return;
	}
	// Tell MS Excel that we use "," as separator
	fprintf(fp,"\"sep=,\"\n");
	// Write table header
	fprintf(fp,"Index,Potential,Current,Frequency,Zreal,Zimag,CR,Status\n");
}


void WriteDataToCSVFile(FILE *fp, MeasureData resultdata, int nDataPoints)
{
	if (fp == NULL)
	{
		printf("Unable to write to CSV file\n");
		fflush(stdout);
		return;
	}
	fprintf(fp,"%d,%f,%f,%f,%f,%f,%s,%s\n", nDataPoints+1, resultdata.potential, resultdata.current, resultdata.frequency, resultdata.zreal, resultdata.zimag, resultdata.cr, resultdata.status);
}
