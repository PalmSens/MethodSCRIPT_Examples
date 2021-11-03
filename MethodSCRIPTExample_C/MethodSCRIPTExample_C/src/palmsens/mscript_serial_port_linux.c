/**
 * \file
 * Serial Port interface implementation for Linux.
 *
 * ----------------------------------------------------------------------------
 *
 * \copyright (c) 2019-2021, PalmSens BV
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - The name of PalmSens may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

SerialPortHandle_t mscript_serial_port_open(char const * serial_port_name)
{
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
	cfsetispeed(&config, B230400);
	cfsetospeed(&config, B230400);

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
