/** 
 * \file
 * Serial Port interface implementation for Windows.
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
#include "mscript_serial_port.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "mscript_debug_printf.h"

SerialPortHandle_t mscript_serial_port_open(char const * port)
{
	assert(port != NULL);

	bool success = true;
	HANDLE handle = INVALID_HANDLE_VALUE;
	DCB dcb;

	// In Windows, "COM1" can be opened as file using file name "\\.\COM1".
	// Create the full file name for the COM port by concatenating "\\.\" and
	// the user-supplied port. Note that that backslashes should be escaped in
	// C, so "\\.\" becomes "\\\\.\\" in code.
	if (strlen(port) > 6) {
		DEBUG_PRINTF("ERROR: expected port in format COMxxx (e.g. COM1 .. COM999).\n");
		success = false;
	}

	if (success) {
		char path[11];
		strcpy(path, "\\\\.\\");
		strcat(path, port);

		// Open serial port in read/write mode,
		handle = CreateFile(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (handle == INVALID_HANDLE_VALUE) {
			DEBUG_PRINTF("ERROR: Failed to open '%s' (error %lu).\n", port, GetLastError());
			success = false;
		}
	}
	if (success) {
		// Configure the serial port connection using device control block (DCB) structure.
		if (!GetCommState(handle, &dcb)) {
			DEBUG_PRINTF("ERROR: Failed to get comm state (error %lu).\n", GetLastError());
			success = false;
		}
	}
	if (success) {
		// Configuration for EmStat Pico is: 230400 bps, 8 data bits, no parity, and 1 stop bit.
		dcb.BaudRate = 230400;
		dcb.ByteSize = 8;
		dcb.Parity = NOPARITY;
		dcb.StopBits = ONESTOPBIT;
		if (!SetCommState(handle, &dcb)) {
			DEBUG_PRINTF("ERROR: Failed to set comm state (error %lu).\n", GetLastError());
			success = false;
		}
	}
	if (success) {
		// Set the communication timeouts (in ms)
		COMMTIMEOUTS timeouts = {
			.ReadIntervalTimeout         =   0, // read timeout between consecutive characters
			.ReadTotalTimeoutMultiplier  =   1, // read timeout per character
			.ReadTotalTimeoutConstant    = 100, // read timeout per read operation
			.WriteTotalTimeoutMultiplier =   1, // write timeout per character
			.WriteTotalTimeoutConstant   = 100, // write timeout per write operation
		};
		if (!SetCommTimeouts(handle, &timeouts)) {
			DEBUG_PRINTF("ERROR: Failed to set comm timeouts (error %lu).\n", GetLastError());
			success = false;
		}
	}

	if (success) {
		DEBUG_PRINTF("Opened and configured serial port on %s.\n", port);
	} else {
		// In case of error, release the acquired resources.
		if (handle != INVALID_HANDLE_VALUE) {
			mscript_serial_port_close(handle);
			handle = INVALID_HANDLE_VALUE;
		}
	}
	return handle;
}

bool mscript_serial_port_close(SerialPortHandle_t handle)
{
	assert(handle != INVALID_HANDLE_VALUE);

	bool success = CloseHandle(handle);
	if (success) {
		DEBUG_PRINTF("Closed serial port.\n");
	} else {
		DEBUG_PRINTF("ERROR: Failed to close serial port (error %lu).\n", GetLastError());
		success = false;
	}
	return success;
}

bool mscript_serial_port_write(SerialPortHandle_t handle, char const * buf)
{
	assert(handle != INVALID_HANDLE_VALUE);
	assert(buf != NULL);

	bool success = true;
	size_t n = strlen(buf);
	DWORD dwBytesWritten;
	if (!WriteFile(handle, buf, n, &dwBytesWritten, NULL)) {
		DEBUG_PRINTF("ERROR: Failed to write to device (error %lu).\n", GetLastError());
		success = false;
	}
	return success;
}

int mscript_serial_port_read(SerialPortHandle_t handle, char * p_character)
{
	assert(handle != INVALID_HANDLE_VALUE);
	assert(p_character != NULL);

	DWORD numberOfBytesRead;
	if (!ReadFile(handle, p_character, 1, &numberOfBytesRead, NULL)) {
		DEBUG_PRINTF("ERROR: Failed to read from device (error %lu).\n", GetLastError());
		return -1;
	}
	return (int)numberOfBytesRead; // 0 or 1
}
