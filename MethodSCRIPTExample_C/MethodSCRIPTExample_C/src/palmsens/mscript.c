/** 
 * \file
 * MethodSCRIPT communication functions.
 * 
 * This file contains:
 *   - some high-level functions to demonstrate how to communicate with a
 *     PalmSens instrument, for example to read the firmware version or send
 *     and execute a script.
 *   - the `mscript_serial_port_read_line()` function, which is shared between
 *     the Windows and Linux implementations of the serial port module.
 *   - functions to translate the data received from a MethodSCRIPT device to
 *     human-readable strings, so they can be printed to the console or file.
 *   - functions to parse and translate the data package response of a
 *     MethodSCRIPT instrument.
 * 
 * Note that there are many possible ways to implement this in C. Below code
 * is a generic solution with limited error checking, intended to demonstrate
 * the basics and to provide a quick start for our customers. Of course, the
 * code could be adjusted to better match a specific use case.
 *
 * ----------------------------------------------------------------------------
 *
 *	\copyright (c) 2021 PalmSens BV
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
#include "mscript.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mscript_debug_printf.h"
#include "mscript_serial_port.h"

/// Maximum number of characters that the EmStat Pico can receive in one line
#define MSCRIPT_WRITE_LINE_MAX_CHARS 128

/// Buffer size for write line buffer (maximum length + 1 for terminating zero character)
#define MSCRIPT_WRITE_LINE_BUF_SIZE (MSCRIPT_WRITE_LINE_MAX_CHARS + 1)

/**
 * Offset value for MethodSCRIPT parameters (see the MethodSCRIPT documentation
 * paragraph 'Measurement data package variables').
 */
#define MSCRIPT_PARAMETER_OFFSET 0x8000000

#if defined(_WIN32)	// Windows (32-bit or 64-bit)

	#include <windows.h> // for GetTickCount() and Sleep()
	static inline uint32_t get_time_ms(void) { return (uint32_t)GetTickCount(); }

#elif defined (__linux__) // Linux

	#include <time.h>
	static void Sleep(int ms)
	{
		div_t dt = div(ms, 1000);
		struct timespec ts;
		ts.tv_sec = dt.quot;
		ts.tv_nsec = dt.rem * 1000000L;
		nanosleep(&ts, NULL);
	}
	static uint32_t get_time_ms(void)
	{
		struct timespec ts;
		assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
		return (uint32_t)((ts.tv_sec * 1000UL) + (ts.tv_nsec / 1000000UL));
	}

#endif

/**
 * Flush the communication.
 * 
 * Some instruments with Bluetooth on older development boards received invalid
 * characters at the start of a connection. These invalid characters can cause
 * communication issues. This function sends a few "dummy" commands to flush
 * any invalid characters in the buffer of the device, and also reads the
 * corresponding responses so they do not interfere the later communication.
 * 
 * Note that this function is not necessary when using a serial port or USB
 * interface.
 */
void mscript_flush_communication(SerialPortHandle_t handle)
{
	for (unsigned int i = 1 ; i <= 3; ++i) {
		DEBUG_PRINTF("Flushing communication (%u/3)...\n", i);
		mscript_serial_port_write(handle, "\n");
		Sleep(100);
		char dummy;
		while (mscript_serial_port_read(handle, &dummy) == 1) { }
	}
}

/**
 * Read one line from the device.
 *
 * \param handle
 *            Handle to the device.
 * \param buf[out]
 *            Buffer to store the received line in. The buffer must be large
 *            enough to receive the complete line (including the newline
 *            character and terminating zero).
 * \param buf_size
 *            Size of the `buf`.
 * \param timeout_ms
 *            Read timeout in milliseconds.
 *
 * \return    `true` if a (possibly empty) line was received and stored in the
 *            buffer;
 * \return    `false` if a read error occurred, no new line character was
 *            received, or the buffer was too small to store the received line.
 */
bool mscript_serial_port_read_line(SerialPortHandle_t handle, char * buf, size_t buf_size,
	uint32_t timeout_ms)
{
	assert(handle != BAD_HANDLE);
	assert(buf != NULL);
	assert(buf_size >= 2);

	uint32_t t0 = get_time_ms();
	size_t number_of_bytes_read = 0;
	for (;;) {
		// Check if buffer is large enough to store another character + '\0'.
		if ((number_of_bytes_read + 1) >= buf_size) {
			DEBUG_PRINTF("ERROR: buffer too small to store received line.\n");
			return false;
		}
		// Read one character.
		char temp = '?';
		int read_result = mscript_serial_port_read(handle, &temp);
		if (read_result == 1) { // successfully read byte
			*buf++ = temp;
			++number_of_bytes_read;
			// Check for end of line.
			if (temp == '\n') {
				*buf = '\0';
				break;
			}
		} else if (read_result == 0) { // timeout
			uint32_t dt = get_time_ms() - t0;
			if (dt >= timeout_ms) {
				DEBUG_PRINTF("ERROR: timeout while reading line.\n");
				return false;
			}
		} else { // -1, error
			return false;
		}
	}

	return true;
}

/**
 * Get the firmware version from the device.
 *
 * \param h_device Handle to the serial port.
 * \param buf buffer to store the firmware version string in
 * \param buf_size size of the buffer
 * 
 * \return `true` on success, `false` on failure
 */
bool mscript_get_firmware_version(SerialPortHandle_t handle, char * buf, size_t buf_size)
{
	// Write "t" command to device.
	bool success = mscript_serial_port_write(handle, "t\n");
	if (!success) {
		return false;
	}

	// The response of the 't' command starts with "t" and ends with "*\n" and
	// can contain newlines (i.e., the response may consist of multiple lines).
	// The code below reads lines until the end sequence ("*\n") is detected,
	// and concatenates all lines into the output buffer. If the buffer is not
	// large enough, an error is returned.
	// NB: See the EmStat Pico communication protocol documentation for a more
	// detailed description of the format of the response.

	// Read first response line.
	char response[MSCRIPT_READ_BUFFER_SIZE];
	success = mscript_serial_port_read_line(handle, response, MSCRIPT_READ_BUFFER_SIZE, 100);
	if (!success) {
		return false;
	}

	// Check if format of response is as expected ("t...\n").
	if (response[0] != 't') {
		DEBUG_PRINTF("ERROR: Unexpected response to firmware version request (first line).\n");
		return false;
	}

	char * src = response + 1; // skip first character ('t')
	for (;;) {
		size_t len = strlen(src);
		size_t src_len = len - 1; // do not copy last character ('\n')

		// Check if buffer is large enough to hold firmware version:
		if (buf_size <= src_len) {
			DEBUG_PRINTF("ERROR: Buffer is too small to hold firmware version.\n");
			return false;
		}

		// Copy the (relevant part of the) response to the buffer.
		memcpy(buf, src, src_len);
		buf += src_len;
		buf_size -= src_len;

		// Check if this is the last response line, by searching for "*\n".
		if (!strcmp(src + src_len - 1, "*\n")) {
			// Zero-terminate the output string and return.
			*buf = '\0';
			break;
		}

		// Read next response line.
		success = mscript_serial_port_read_line(handle, response, MSCRIPT_READ_BUFFER_SIZE, 100);
		if (!success) {
			return false;
		}

		// Add a delimiter character to the buffer to separate the lines
		assert(buf_size > 0);
		*buf++ = ' ';
		--buf_size;

		// Set pointer and size for the next iteration (will be copied at start of loop)
		src = response;
	}

	return true;
}

/**
 * Get the device (instrument) type from the firmware version string.
 * 
 * \param firmware_version The firmware version string, which should have been
 *                         retrieved using `mscript_get_firmware_version()`.
 * 
 * \return The device type.
 */
DeviceType_t mscript_get_device_type(char const * firmware_version)
{
	// Derive device type from version string.
	if (!strncmp(firmware_version, "espico", 6)) {
		return EMSTAT_PICO;
	} else if (!strncmp(firmware_version, "es4_lr", 6)) {
		return EMSTAT4_LR;
	} else if (!strncmp(firmware_version, "es4_hr", 6)) {
		return EMSTAT4_HR;
	} else if (!strncmp(firmware_version, "mes4hr", 6)) {
		return MULTI_EMSTAT4_LR;
	} else if (!strncmp(firmware_version, "mes4hr", 6)) {
		return MULTI_EMSTAT4_HR;
	}
	return UNKNOWN_DEVICE;
}

static char const * const DEVICE_TYPE_NAMES[] = {
	[UNKNOWN_DEVICE]   = "Unknown device",
	[EMSTAT_PICO]      = "EmStat Pico",
	[EMSTAT4_LR]       = "EmStat4 LR",
	[EMSTAT4_HR]       = "EmStat4 HR",
	[MULTI_EMSTAT4_LR] = "MultiEmStat4 LR",
	[MULTI_EMSTAT4_HR] = "MultiEmStat4 HR",
};

/**
 * Get the device name from a device type.
 * 
 * \param device_type The device type, which should have been retrieved using
 *                    `mscript_get_device_type()`.
 *
 * \return The device type as printable string.
 */
char const * mscript_get_device_type_name(DeviceType_t device_type)
{
	if (device_type < (sizeof(DEVICE_TYPE_NAMES) / sizeof(DEVICE_TYPE_NAMES[0]))) {
		return DEVICE_TYPE_NAMES[device_type];
	}
	return DEVICE_TYPE_NAMES[UNKNOWN_DEVICE];
}

/**
 * Send a MethodSCRIPT from file to the device.
 *
 * \param h_device Handle to the serial port.
 * \param path Path to the MethodSCRIPT file to be read and sent.
 * 
 * \return `true` on success, `false` on failure
 */
bool mscript_send_file(SerialPortHandle_t handle, char const * path)
{
	assert(handle != BAD_HANDLE);
	assert(path != NULL);

	bool success = true;
	// Allocate buffer for one line
	char line[MSCRIPT_WRITE_LINE_BUF_SIZE];

	// Open file.
	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		DEBUG_PRINTF("ERROR: Could not open script file %s: %s\n", path, strerror(errno));
		success = false;
	}

	// Read the file and send it to the device, line by line.
	while (success) {
		// Read line from file.
		if (fgets(line, MSCRIPT_WRITE_LINE_BUF_SIZE, fp) == NULL) {
			// fgets() returns NULL on end-of-file or error.
			// Check if EOF was the reason. If not, it was an error.
			if (!feof(fp)) {
				DEBUG_PRINTF("ERROR while reading from script file.\n");
				success = false;
			}
			break;
		}
		// Send line to the device.
		success = mscript_serial_port_write(handle, line);
	}

	if (success) {
		DEBUG_PRINTF("Successfully sent script file to device.\n");
	}

	// Cleanup: close file if it was opened successfully.
	if (fp != NULL) {
		if (fclose(fp)) {
			DEBUG_PRINTF("ERROR: Failed to close script file: %s\n", strerror(errno));
		}
		fp = NULL;
	}

	return success;
}

static void mscript_clear_sub_package(MscriptSubPackage_t * subpackage)
{
	subpackage->value = 0;
	subpackage->variable_type = 0; // MSCR_STR_TO_VT("aa");
	subpackage->metadata.status = -1;
	subpackage->metadata.range = -1;
}

static void mscript_clear_data_package(MscriptDataPackage_t * package)
{
	package->nr_of_sub_packages = 0;
	for (int i = 0; i < MSCRIPT_MAX_SUB_PACKAGES_PER_LINE; ++i) {
		mscript_clear_sub_package(&package->sub_packages[i]);
	}
}

static double get_si_prefix_value(char prefix)
{
	switch (prefix)
	{
	case 'a': return 1e-18; // atto
	case 'f': return 1e-15; // femto
	case 'p': return 1e-12; // pico
	case 'n': return 1e-9;  // nano
	case 'u': return 1e-6;  // micro
	case 'm': return 1e-3;  // milli
	case ' ': return 1;
	case 'k': return 1e3;   // kilo
	case 'M': return 1e6;   // mega
	case 'G': return 1e9;   // giga
	case 'T': return 1e12;  // tera
	case 'P': return 1e15;  // peta
	case 'E': return 1e18;  // exa
							// special case: 'i' is not an SI prefix, but is used for integer values
	case 'i': return 1;
	}
	return 0;
}

static double get_parameter_value(char const * param)
{
	// Special case: Not-a-Number (NaN)
	if (!strncmp(param, "     nan", 8)) {
		return NAN;
	}

	// Determine the integer value by converting the 7 hexadecimal characters
	// and subtracting the offset. We first copy the digits to a new string
	// because the next character (the SI unit) could be interpreted as
	// hexadecimal value if it is an 'a' (atto), 'f' (femto) or 'E' (exa)
	// character.
	char value_str[8];
	strncpy(value_str, param, 7);
	value_str[7] = '\0';
	long value = strtol(value_str, NULL, 16)- MSCRIPT_PARAMETER_OFFSET;

	// Now lookup the value for the SI prefix (at position 7) and return the
	// scaled value as a floating point number.
	return value * get_si_prefix_value(param[7]);
}

static void parse_metadata(char const * remainder, MscriptSubPackage_t * sub_package)
{
	// Process meta data
	do {
		if (*remainder != ',') {
			return;
		}
		switch (remainder[1])
		{
		case '1':
			sub_package->metadata.status = strtol(remainder + 2, NULL, 16);
			break;
		case '2':
			sub_package->metadata.range = strtol(remainder + 2, NULL, 16);
			break;
		}
		remainder = strpbrk(remainder + 1, ",;");
	} while (remainder != NULL);
}

static void parse_sub_package(char const * token, MscriptSubPackage_t * sub_package)
{
	// Split the parameter identifier string
	sub_package->variable_type = MSCRIPT_VARTYPE_STR_TO_INT(token);

	// Split the parameter value string
	// Retrieve the actual parameter value
	sub_package->value = get_parameter_value(token + 2);

	// The rest of the parameter is further parsed to get metadata values
	parse_metadata(token + 10, sub_package);
}

/**
 * Parse a data package received from a MethodSCRIPT device.
 *
 * \param response The reponse line containing a MethodSCRIPT data package.
 * \param package Package structure to store the parsed data in.
 * 
 * \return `true` on success, `false` on failure
 */
bool parse_data_package(char const * response, MscriptDataPackage_t * package)
{
	mscript_clear_data_package(package);

	// Set start position for string to tokenize to `response + 1` to skip
	// the first character ('P').
	if (response[1] != '\n') {
		unsigned int i = 0;
		for (;;) {
			// Parse the sub package
			parse_sub_package(response + 1, &package->sub_packages[i++]);
			response = strchr(response + 1, ';');
			if (response == NULL) {
				// No more sub packages to be parsed.
				break;
			}
			if (i >= MSCRIPT_MAX_SUB_PACKAGES_PER_LINE) {
				// There are more sub packages to be parsed but the array
				// is already full.
				DEBUG_PRINTF("WARNING: Too many sub packages (> %u). Remaining sub "
					         "packages are ignored.\n",
					         (unsigned int)MSCRIPT_MAX_SUB_PACKAGES_PER_LINE);
				break;
			}
		}
		package->nr_of_sub_packages = i;
	}

	return true;
}

/**
 * Get a printable string representation of the variable type.
 *
 * \param vartype The variable type, as received from the device.
 * 
 * \return The variable type as printable string.
 */
char const * mscript_vartype_to_string(unsigned int vartype)
{
	switch(vartype)
	{
	case MSCRIPT_VARTYPE_UNKNOWN:            return "UNKNOWN VAR TYPE";
	case MSCRIPT_VARTYPE_POTENTIAL:          return "Potential";
	case MSCRIPT_VARTYPE_POTENTIAL_CE:       return "Potential_CE_vs_GND";
	case MSCRIPT_VARTYPE_POTENTIAL_SE:       return "Potential_SE_vs_GND";
	case MSCRIPT_VARTYPE_POTENTIAL_RE:       return "Potential_RE_vs_GND";
	case MSCRIPT_VARTYPE_POTENTIAL_WE_VS_CE: return "Potential_WE_vs_CE";
	case MSCRIPT_VARTYPE_POTENTIAL_AIN0:     return "Potential_AIN0";
	case MSCRIPT_VARTYPE_POTENTIAL_AIN1:     return "Potential_AIN1";
	case MSCRIPT_VARTYPE_POTENTIAL_AIN2:     return "Potential_AIN2";
	case MSCRIPT_VARTYPE_POTENTIAL_AIN3:     return "Potential_AIN3";
	case MSCRIPT_VARTYPE_POTENTIAL_AIN4:     return "Potential_AIN4";
	case MSCRIPT_VARTYPE_POTENTIAL_AIN5:     return "Potential_AIN5";
	case MSCRIPT_VARTYPE_POTENTIAL_AIN6:     return "Potential_AIN6";
	case MSCRIPT_VARTYPE_POTENTIAL_AIN7:     return "Potential_AIN7";
	case MSCRIPT_VARTYPE_CURRENT:            return "Current";
	case MSCRIPT_VARTYPE_PHASE:              return "Phase";
	case MSCRIPT_VARTYPE_IMP:                return "Imp";
	case MSCRIPT_VARTYPE_ZREAL:              return "Zreal";
	case MSCRIPT_VARTYPE_ZIMAG:              return "Zimag";
	case MSCRIPT_VARTYPE_EIS_TDD_E:          return "EIS_TDD_E";
	case MSCRIPT_VARTYPE_EIS_TDD_I:          return "EIS_TDD_I";
	case MSCRIPT_VARTYPE_EIS_FS:             return "EIS_FS";
	case MSCRIPT_VARTYPE_EIS_E_AC:           return "EIS_E_AC";
	case MSCRIPT_VARTYPE_EIS_E_DC:           return "EIS_E_DC";
	case MSCRIPT_VARTYPE_EIS_I_AC:           return "EIS_I_AC";
	case MSCRIPT_VARTYPE_EIS_I_DC:           return "EIS_I_DC";
	case MSCRIPT_VARTYPE_CELL_SET_POTENTIAL: return "Cell_set_potential";
	case MSCRIPT_VARTYPE_CELL_SET_CURRENT:   return "Cell_set_current";
	case MSCRIPT_VARTYPE_CELL_SET_FREQUENCY: return "Cell_set_frequency";
	case MSCRIPT_VARTYPE_CELL_SET_AMPLITUDE: return "Cell_set_amplitude";
	case MSCRIPT_VARTYPE_CHANNEL:            return "Channel";
	case MSCRIPT_VARTYPE_TIME:               return "Time";
	case MSCRIPT_VARTYPE_PIN_MSK:            return "Pin_msk";
	case MSCRIPT_VARTYPE_TEMPERATURE:        return "Temperature";
	case MSCRIPT_VARTYPE_CURRENT_GENERIC1:   return "Current_generic1";
	case MSCRIPT_VARTYPE_CURRENT_GENERIC2:   return "Current_generic2";
	case MSCRIPT_VARTYPE_CURRENT_GENERIC3:   return "Current_generic3";
	case MSCRIPT_VARTYPE_CURRENT_GENERIC4:   return "Current_generic4";
	case MSCRIPT_VARTYPE_POTENTIAL_GENERIC1: return "Potential_generic1";
	case MSCRIPT_VARTYPE_POTENTIAL_GENERIC2: return "Potential_generic2";
	case MSCRIPT_VARTYPE_POTENTIAL_GENERIC3: return "Potential_generic3";
	case MSCRIPT_VARTYPE_POTENTIAL_GENERIC4: return "Potential_generic4";
	case MSCRIPT_VARTYPE_MISC_GENERIC1:      return "Misc_generic1";
	case MSCRIPT_VARTYPE_MISC_GENERIC2:      return "Misc_generic2";
	case MSCRIPT_VARTYPE_MISC_GENERIC3:      return "Misc_generic3";
	case MSCRIPT_VARTYPE_MISC_GENERIC4:      return "Misc_generic4";
	}
	return "Undefined variable type";
}

/**
 * Get a printable string representation of the status metadata.
 *
 * \param status_flag A flag (only 1 bit!) of the status metadata.
 * 
 * \note The metadata "status" is a bit mask of 4 bits. Each bit is set
 *       or cleared independently. This function is not guaranteed to
 *       work with the value of the "status" field as argument. Instead,
 *       it should be called for each bit indivually (or with the value 0).
 * 
 * \return The meaning of the status flag as printable string.
 */
char const * mscript_metadata_status_to_string(unsigned int status_flag)
{
	switch (status_flag)
	{
	case MSCRIPT_STATUS_OK:               return "OK";
	case MSCRIPT_STATUS_TIMING_ERROR:     return "Timing not met!";
	case MSCRIPT_STATUS_OVERLOAD:         return "Overload";
	case MSCRIPT_STATUS_UNDERLOAD:        return "Underload";
	case MSCRIPT_STATUS_OVERLOAD_WARNING: return "Overload warning";
	}
	return "???"; // error, should not happen if at most one bit is set.
}

/**
 * Get a printable string representation of the range.
 * 
 * Note that this metadata variable was previously called "current range".
 * However, for some variable types the range is actually a potential range.
 * This is why the variable type is also passed as argument to this function.
 *
 * \param device_type The type of device (instrument).
 * \param variable_type The variable type (see MethodSCRIPT documentation).
 * \param range The range, as received from the device.
 * 
 * \return The actual range as printable string.
 */
char const * mscript_metadata_range_to_string(DeviceType_t device_type, 
	unsigned int variable_type, int range)
{
	switch (device_type)
	{
	case EMSTAT_PICO:
		// Translate current ranges used in EmStat Pico.
		switch (range)
		{
		case   0: return "100 nA";
		case   1: return   "2 uA";
		case   2: return   "4 uA";
		case   3: return   "8 uA";
		case   4: return  "16 uA";
		case   5: return  "32 uA";
		case   6: return  "63 uA";
		case   7: return "125 uA";
		case   8: return "250 uA";
		case   9: return "500 uA";
		case  10: return   "1 mA";
		case  11: return   "5 mA";
		case 128: return "100 nA (High speed)";
		case 129: return   "1 uA (High speed)";
		case 130: return   "6 uA (High speed)";
		case 131: return  "13 uA (High speed)";
		case 132: return  "25 uA (High speed)";
		case 133: return  "50 uA (High speed)";
		case 134: return "100 uA (High speed)";
		case 135: return "200 uA (High speed)";
		case 136: return   "1 mA (High speed)";
		case 137: return   "5 mA (High speed)";
		}
		break;

	case EMSTAT4_LR:
	case EMSTAT4_HR:
	case MULTI_EMSTAT4_LR:
	case MULTI_EMSTAT4_HR:
		// For EmStat4 series instruments, the range can be a current range or
		// potential range, depending on the variable type.
		switch (variable_type)
		{
		case MSCRIPT_VARTYPE_POTENTIAL:
		case MSCRIPT_VARTYPE_ZIMAG:
			// These variable types define a potential range.
			// Translate potential ranges used in EmStat4 instruments.
			switch (range)
			{
			case 2: return  "50 mV";
			case 3: return "100 mV";
			case 4: return "200 mV";
			case 5: return "500 mV";
			case 6: return   "1 V";
			}
			break;
		default:
			// Translate current ranges used in EmStat4 instruments.
			switch (range)
			{
			// (Multi)EmStat4 LR only:
			case   3: return   "1 nA";
			case   6: return  "10 nA";
			// (Multi)EmStat4 LR/HR:
			case   9: return "100 nA";
			case  12: return   "1 uA";
			case  15: return  "10 uA";
			case  18: return "100 uA";
			case  21: return   "1 mA";
			case  24: return  "10 mA";
			// (Multi)EmStat4 HR only:
			case  27: return "100 mA";
			}
		}
		break;

	case UNKNOWN_DEVICE:
		break;
	}

	return "Unknown/invalid range value";
}
