#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h> // Serial port interface on Linux

// PalmSens includes
#include "MethodSCRIPTExample.h" // Used for serial port settings
#include "SerialPort.h"


#define FAILURE 0
#define SUCCESS 1

int fd; // File descriptor for serial port


// Look up element for coversion between int and speed_t
typedef struct termios_baud_lut_t {
	int baud;
	speed_t setting;
} termios_baud_lut_t;


// Look up table for converting baudrate (int) to TERMIOS speed_t
static const termios_baud_lut_t termios_baud_lut[] =  {
		{      0, B0      },
		{     50, B50     },
		{     75, B75     },
		{    110, B110    },
		{    134, B134    },
		{    150, B150    },
		{    200, B200    },
		{    300, B300    },
		{    600, B600    },
		{   1200, B1200   },
		{   1800, B1800   },
		{   2400, B2400   },
		{   4800, B4800   },
		{   9600, B9600   },
		{  19200, B19200  },
		{  38400, B38400  },
		{  57600, B57600  },
		{ 115200, B115200 },
		{ 230400, B230400 },
		{ 460800, B460800 },
		{ 500000, B500000 },
		{ 576000, B576000 },
		{ 921600, B921600 },
		{1000000, B1000000},
		{1152000, B1152000},
		{1500000, B1500000},
		{2000000, B2000000},
		{2500000, B2500000},
		{3000000, B3000000},
		{3500000, B3500000},
		{4000000, B4000000}
};

#define baud_lut_size (sizeof(termios_baud_lut) / sizeof(termios_baud_lut_t))


//
//
//
speed_t baud_to_termios(int request_baud)
{
	termios_baud_lut_t current_baud;

	for (int i = 1 ; i < baud_lut_size; i++)
	{
		current_baud = termios_baud_lut[i];
		if (current_baud.baud == request_baud)
			return termios_baud_lut[i].setting;
		if (current_baud.baud > request_baud)
		{
			current_baud = termios_baud_lut[i - 1];
			break;
		}
	}

	// Requested baud was higher than we can create
	printf("WARNING: exact baudrate of %d cannot be selected. Using %d instead.\n", request_baud, current_baud.baud);
	return current_baud.setting;
}


//
//
//
int OpenSerialPort()
{
	fd = open(SERIAL_PORT_NAME, O_RDWR | O_NOCTTY | O_NDELAY);
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
	// Set baudrate for both input and output
	//
	speed_t baud_config = baud_to_termios(BAUD_RATE);

	cfsetispeed(&config, baud_config);
	cfsetospeed(&config, baud_config);

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


//
//
//
int WriteToDevice(char tx_char)
{
	int n = write(fd, &tx_char, 1);
	if (n < 0)
	  return FAILURE;
	return SUCCESS;
}


//
//
//
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


//
//
//
int CloseSerialPort()
{
	close(fd);
	return SUCCESS;
}

