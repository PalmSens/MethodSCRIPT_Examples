

#include <windows.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "SerialPort.h"


HANDLE hCom;				// Serial port handle
DWORD dwBytesWritten = 0;	// Number of bytes written variable for serial WriteFile function

//
//
//
int OpenSerialPort(char *serial_port_name, uint32_t baudrate)
{
	DCB dcb = { 0 };
	BOOL fSuccess;
	hCom = CreateFile(serial_port_name, GENERIC_READ | GENERIC_WRITE, 0,  		// must be opened with exclusive-access
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

	dcb.BaudRate = baudrate; 		// set the baud rate 230400 bps
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


//
//
//
int WriteToDevice(char c)
{
	char writeChar[2] = {c,'\0'};
	if (WriteFile(hCom, writeChar, 1, &dwBytesWritten, NULL))
	{
		return 1;
	}
	return 0;
}


//
//
//
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


//
//
//
int CloseSerialPort()
{
	if (hCom != NULL)
		CloseHandle(hCom);
	return 0;
}
