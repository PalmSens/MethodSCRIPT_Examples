/**
 * \file
 * Serial Port interface implementation for Linux.
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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "mscript_debug_printf.h"

static speed_t get_speed(int baud)
{
	switch (baud) {
	case 50: return B50;
	case 75: return B75;
	case 110: return B110;
	case 134: return B134;
	case 150: return B150;
	case 200: return B200;
	case 300: return B300;
	case 600: return B600;
	case 1200: return B1200;
	case 1800: return B1800;
	case 2400: return B2400;
	case 4800: return B4800;
	case 9600: return B9600;
	case 19200: return B19200;
	case 38400: return B38400;
	case 57600: return B57600;
	case 115200: return B115200;
	case 230400: return B230400;
	case 460800: return B460800;
	case 500000: return B500000;
	case 576000: return B576000;
	case 921600: return B921600;
	case 1000000: return B1000000;
	case 1152000: return B1152000;
	case 1500000: return B1500000;
	case 2000000: return B2000000;
	case 2500000: return B2500000;
	case 3000000: return B3000000;
	case 3500000: return B3500000;
	case 4000000: return B4000000;
	default: return B0;
	}
}


SerialPortHandle_t mscript_serial_port_open(char const * serial_port_name, int baudrate)
{
	speed_t speed = get_speed(baudrate);
	if (speed == B0)
	{
		DEBUG_PRINTF("Invalid baud rate provided: %d\n", baudrate);
		return BAD_HANDLE;
	}

	int fd = open(serial_port_name, O_RDWR | O_NOCTTY);
	if (fd == -1)
	{
		DEBUG_PRINTF("Unable to open serial port: %s\n", strerror(errno));
		return BAD_HANDLE;
	}

	// Get the current configuration of the serial interface
	struct termios config;
	if (tcgetattr(fd, &config) != 0)
	{
		DEBUG_PRINTF("Unable to get serial port configuration: %s\n", strerror(errno));
		return BAD_HANDLE;
	}

	// Set the baudrate for both input and output
	cfsetispeed(&config, speed);
	cfsetospeed(&config, speed);

	// Input flags: Turn off input processing and flow control
	config.c_iflag &= ~(IXON | IXOFF | IXANY);

	// Local mode flags: disable echo and put the interface in non-canonical mode (transmit without buffering).
	config.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	// Output flags: Turn off output processing
	config.c_oflag &= ~OPOST;

	// Control mode flags: Turn off output processing and act as null-modem
	//                     Clear character size mask, no parity checking,
	//                     No output processing, force 8 bit input
	config.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
	config.c_cflag |= CS8 |CREAD | CLOCAL;

	// Set read timeout
	config.c_cc[VMIN] = 0;
	config.c_cc[VTIME] = 1; // 100 ms

	if (tcsetattr(fd, TCSANOW, &config) != 0) {
		DEBUG_PRINTF("Unable to set serial port configuration: %s\n", strerror(errno));
		return BAD_HANDLE;
	}

	return fd;
}

bool mscript_serial_port_close(SerialPortHandle_t handle)
{
	assert(handle >= 0);

	int retval = close(handle);
	if (retval == -1) {
		DEBUG_PRINTF("ERROR: Failed to close serial port: %s\n", strerror(errno));
		return false;
	}
	DEBUG_PRINTF("Closed serial port.\n");
	return true;
}

bool mscript_serial_port_write(SerialPortHandle_t handle, char const * buf)
{
	assert(handle >= 0);
	assert(buf != NULL);

	bool success = true;
	size_t n = strlen(buf);
	ssize_t bytes_written = write(handle, buf, n);
	if (bytes_written < (ssize_t)n) {
		if (bytes_written == -1) {
			DEBUG_PRINTF("ERROR: Failed to write to device: %s\n", strerror(errno));
		} else {
			DEBUG_PRINTF("ERROR: Not all bytes were written to the device.\n");
		}
		success = false;
	}
	return success;
}

int mscript_serial_port_read(SerialPortHandle_t handle, char * p_character)
{
	assert(handle >= 0);
	assert(p_character != NULL);

	ssize_t bytes_read = read(handle, p_character, 1);
	if (bytes_read == -1) {
		if (errno == EAGAIN) { // timeout
			return 0;
		}
		DEBUG_PRINTF("ERROR: Failed to read from device: %s\n", strerror(errno));
		return -1;
	}
	return (int)bytes_read; // 0 or 1
}
