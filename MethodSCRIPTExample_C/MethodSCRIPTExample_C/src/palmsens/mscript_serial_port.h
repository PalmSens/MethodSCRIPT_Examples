/** 
 * \file
 * Serial Port interface.
 * 
 * This is the common header file for the serial interface. This file can be
 * included in both Windows and Linux applications. However, the implementation
 * for each OS is different.
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
#pragma once

#include <stdbool.h>
#include <stddef.h>

#if defined(_WIN32)	// Windows (32-bit or 64-bit)
	#include <windows.h>
	typedef HANDLE SerialPortHandle_t;
	#define SerialPortHandle_t HANDLE
	#define BAD_HANDLE INVALID_HANDLE_VALUE
#elif defined (__linux__) // Linux
	typedef int SerialPortHandle_t;
	#define SerialPortHandle_t int
	#define BAD_HANDLE -1
#else // Other (unsupported) operating system.
	#error "Unsupported platform."
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Open the serial port connection to the device.
 * 
 * \param serial_port_name name of the serial port, e.g., "COM1" on Windows
 *                         or "/dev/ttyUSB0" on Linux
 *
 * \return a handle on success, or `BAD_HANDLE` on failure.
 */
SerialPortHandle_t mscript_serial_port_open(char const * serial_port_name);

/**
 * Write data to the device.
 *
 * \param handle a valid handle to the serial port connection
 * \param buf a zero-terminated string
 * 
 * \return `true` on success, `false` on failure
 */
bool mscript_serial_port_write(SerialPortHandle_t handle, char const * buf);

/**
 * Read a character read from the device.
 *
 * \param handle a valid handle to the serial port connection
 * \param p_character pointer to character where the result will be stored
 * 
 * \return 1 on success, 0 on timeout, -1 on error
 */
int mscript_serial_port_read(SerialPortHandle_t handle, char * p_character);

/**
 * Close the serial port.
 *
 * \param handle a valid handle to the serial port connection
 * 
 * \return `true` on success, `false` on failure
*/
bool mscript_serial_port_close(SerialPortHandle_t handle);

#ifdef __cplusplus
} // extern "C"
#endif
