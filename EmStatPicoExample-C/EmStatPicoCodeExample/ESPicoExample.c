/* ----------------------------------------------------------------------------
 *         PalmSens Method SCRIPT SDK
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
 *  Implementation of Method SCRIPT. See header file for more information.
 */
#include "ESPicoExample.h"

HANDLE hCom;
DWORD dwBytesWritten = 0;
DWORD dwBytesRead;
PSComm psComm;
MeasureData data;

int main(int argc, char *argv[])
{
	RetCode code = PSCommInit(&psComm, &WriteToDevice, &ReadFromDevice);
	int continueParsing;
	if (code == CODE_OK)
	{
		int isOpen = OpenSerialPort();
		if(isOpen)
		{
			char buff[PATH_MAX];
			char *currentDirectory = getcwd(buff, PATH_MAX);
			char* filePath = "\\EsPicoScriptFiles\\meas_swv_test.txt"; //"LSV_test_script.txt";
			char* combinedFilePath = strcat(currentDirectory, filePath);
			if(ReadScriptFile(combinedFilePath))
			{
				printf("\nScript file sent to EmStat Pico.\n");
				do
				{
					code = ReceivePackage(&psComm, &data);
					continueParsing = DisplayResults(code);
				}while(continueParsing == 1);
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
	timeouts.ReadIntervalTimeout = 1000; 			// in milliseconds
	timeouts.ReadTotalTimeoutConstant = 1000; 		// in milliseconds
	timeouts.WriteTotalTimeoutConstant = 50; 		// in milliseconds
	timeouts.WriteTotalTimeoutMultiplier = 10; 		// in milliseconds

	if (!SetCommTimeouts(hCom, &timeouts)) {
		printf("SetCommState failed with error %lu.\n", GetLastError());
	}
	fflush(stdout);

	//Verify if the connected device is EmStat Pico.
	fSuccess = VerifyEmStatPico();
	if(fSuccess)
	{
		printf("Serial port successfully connected to EmStat Pico.\n");
		return 1;
	}
	printf("Could not connect to EmStat Pico.\n");
	return 0;
}

BOOL VerifyEmStatPico()
{
	//Send the command to get version string and verify if port is connected to Emstat pico.
	char versionString[30];
	RetCode code;
	BOOL isConnected = 0;
	WriteStr(&psComm, CMD_VERSION_STRING);
	do{
		code = ReadBuf(&psComm, versionString);
		if(strstr(versionString, "*\n") != NULL && strstr(versionString, "esp") != NULL)
			isConnected = 1;
		if(code == CODE_RESPONSE_END)
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
	char tempChar; 							//Temporary character used for reading
	DWORD noBytesRead;
	ReadFile(hCom, 							//Handle of the Serial port
			&tempChar, 						//Temporary character
			sizeof(tempChar),				//Size of TempChar
			&noBytesRead, 					//Number of bytes read
			NULL);
	return (int)tempChar;
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
		WriteStr(&psComm, str);
	}
	fclose(fp);
	return 1;
}

int DisplayResults(RetCode code)
{
	int continueParsing = 1;
	int nDataPoints = 0;
	if(code == CODE_RESPONSE_BEGIN)
	{
	  //do nothing
	}
	else if(code == CODE_MEASURING)
	{
	  printf("\nMeasuring... \n");
	}
	else if(code == CODE_OK)
	{
	  if(nDataPoints == 0)
		  printf("\nReceiving measurement response:\n");
	  //Received valid package, print it.
	  printf("\n %d \t", ++nDataPoints);
	  printf("E(V): %6.3f \t", data.potential);
	  fflush(stdout);
	  printf("i(A) : %11.3E \t", data.current);
	  printf("Status: %-15s  ", data.status);
	  printf("CR: %s ", data.cr);
	  fflush(stdout);
	}
	else if(code == CODE_MEASUREMENT_DONE)
	{
	  printf("\nMeasurement completed. ");
	}
	else if(code == CODE_RESPONSE_END)
	{
	   printf("%d data point(s) received.", nDataPoints);
	   fflush(stdout);
	   continueParsing = 0;
	}
	else
	{
	  //Failed to parse or identify package.
	  printf("\nFailed to parse package: %d\n", code);
	  continueParsing = 0;
	}
	return continueParsing;
}
