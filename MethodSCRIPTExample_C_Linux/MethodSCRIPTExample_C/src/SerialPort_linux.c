#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h> // Serial port interface on Linux


#include "MethodSCRIPTExample.h" // Used for serial port settings
#include "SerialPort.h"



int fd; // File descriptor for serial port



int OpenSerialPort()
{
	fd = open(PORT_NAME, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd == -1)
	{
		printf("Unable to open serial port.\n");
		return FAILURE;
	}

	struct termios config; // Serial port configuration

	//
	// Get the current configuration of the serial interface
	//
	if(tcgetattr(fd, &config) < 0)
	{
		printf("Unable to get initial serial port configuration\n");
		return FAILURE;
	}

	//
	// Set both input and output to 230400 baud
	//
	cfsetispeed(&config, B230400);
	cfsetospeed(&config, B230400);

	//
	// Input flags - Turn off input processing and flow control
	//
	config.c_iflag &= ~(IXON | IXOFF | IXANY);

	//
	// Local mode flags - disable echo and put the interface in non-canonical mode (transmit without buffering).
	//
	config.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	//
	// Output flags - Turn off output processing
	config.c_oflag &= ~OPOST;

	//
	// Control mode flags - Turn off output processing and act as null-modem
	//
	// clear character size mask, no parity checking,
	// no output processing, force 8 bit input
	//
	config.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
	config.c_cflag |= CS8 |CREAD | CLOCAL;


	if (tcsetattr(fd, TCSANOW, &config) != 0)
	{
		printf("Unable to set serial port configuration\n");
		return FAILURE;
	}

	return SUCCESS;
}


int WriteToDevice(char tx_char)
{
	int n = write(fd, &tx_char, 1);
	if (n < 0)
	  return FAILURE;
	return SUCCESS;
}


int ReadFromDevice()
{
	char rx_char;
	int bytes_read = read(fd, &rx_char, 1); /* Read the data */

	if (bytes_read > 0)
		return rx_char;
	else if (bytes_read == 0)
		return 0;
	else
		return -1;
}


int CloseSerialPort()
{
	close(fd);
	return SUCCESS;
}

